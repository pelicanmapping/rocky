


// https://learnopengl.com/PBR/Lighting
const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void atmos_ground_frag_main(inout vec4 color)
{
    // SRGB to linear for PBR compute:
    vec3 albedo = pow(color.rgb, vec3(2.2));

    vec3 N = normalize(vp_Normal);
    vec3 V = normalize(-vp_VertexView);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, vec3(oe_pbr.metal));

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < OE_NUM_LIGHTS; ++i)
    {
        // per-light radiance:
        vec3 L = normalize(osg_LightSource[i].position.xyz - vp_VertexView);
        vec3 H = normalize(V + L);
        //float distance = length(osg_LightSource[i].position.xyz - atmos_vert);
        //float attenuation = 1.0 / (distance * distance);
        vec3 radiance = vec3(1.0); // osg_LightSource[i].diffuse.rgb * attenuation;

        //radiance *= atmos_atten;

        // cook-torrance BRDF:
        float NDF = DistributionGGX(N, H, oe_pbr.roughness);
        float G = GeometrySmith(N, V, L, oe_pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - oe_pbr.metal;

        float NdotL = max(dot(N, L), 0.0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL;
        vec3 specular = numerator / max(denominator, 0.001);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = osg_LightSource[0].ambient.rgb * albedo * oe_pbr.ao;

    color.rgb = ambient + Lo;

    // tone map:
    color.rgb = color.rgb / (color.rgb + vec3(1.0));

    // add in the haze
    color.rgb += pow(atmos_color, vec3(2.2)); // add in the (SRGB) color

    // exposure:
    color.rgb = 1.0 - exp(-oe_sky_exposure * color.rgb);

    // brightness and contrast
    color.rgb = ((color.rgb - 0.5) * oe_pbr.contrast + 0.5) * oe_pbr.brightness;

    // linear back to SRGB
    color.rgb = pow(color.rgb, vec3(1.0 / 2.2));
}
