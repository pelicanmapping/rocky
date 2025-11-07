#pragma import_defines(ROCKY_ATMOSPHERE)

// from VSG's view-dependent state
layout(set = 1, binding = 0) uniform VSG_Lights {
    vec4 pack[64];
} vsg_lights;

//#ifdef ROCKY_ATMOSPHERE
//layout(location = 15) in vec3 atmos_color;
//#endif

// TODO - this will eventually come from a material map
struct PBR {
    float roughness;
    float metal;
    float ao;
} pbr;

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

vec4 apply_lighting(in vec4 color, in vec3 vertex_view, in vec3 normal)
{
    // temp:
    pbr.ao = 1.0;
    pbr.roughness = 0.75;
    pbr.metal = 0.0;
    const float exposure = 3.3;

    vec3 albedo = color.rgb;

    vec3 N = normalize(normal);
    vec3 V = normalize(-vertex_view);

    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, vec3(pbr.metal));

    vec3 Lo = vec3(0.0);
    vec3 ambient = vec3(0.0); // vec3(0.013);

    vec4 light_counts = vsg_lights.pack[0];
    int ambient_count = int(light_counts[0]);
    int directional_count = int(light_counts[1]);
    int point_count = int(light_counts[2]);
    int spot_count = int(light_counts[3]);
    int total_light_count = ambient_count + directional_count + point_count + spot_count;
    int index = 1;

    for (int i = 0; i < ambient_count; ++i)
    {
        vec4 light_color = vsg_lights.pack[index++];
        ambient += light_color.rgb * light_color.a;
    }

    for (int i = 0; i < directional_count; ++i)
    {
        // For now this is a copy of the point-light code.

        vec3 light_color = vsg_lights.pack[index++].rgb;
        vec3 position = vsg_lights.pack[index++].xyz;

        // per-light radiance:
        vec3 L = normalize(position - vertex_view);
        vec3 H = normalize(V + L);
        vec3 radiance = light_color;

        // cook-torrance BRDF:
        float D = DistributionGGX(N, H, pbr.roughness);
        float G = GeometrySmith(N, V, L, pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NdotL = max(dot(N, L), 0.0);

        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.001; // avoid division by zero
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - pbr.metal;

        vec3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    for (int i = 0; i < point_count; ++i)
    {
        vec3 light_color = vsg_lights.pack[index++].rgb;
        vec3 position = vsg_lights.pack[index++].xyz;

        // per-light radiance:
        vec3 L = normalize(position - vertex_view);
        vec3 H = normalize(V + L);
        vec3 radiance = light_color;

        // cook-torrance BRDF:
        float D = DistributionGGX(N, H, pbr.roughness);
        float G = GeometrySmith(N, V, L, pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NdotL = max(dot(N, L), 0.0);

        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.001; // avoid division by zero
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - pbr.metal;

        vec3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

#if 0
    for (int i = 0; i < spot_count; ++i)
    {
        vec4 light_color = vsg_lights.pack[index++];
        vec4 position_cosInnerAngle = vsg_lights.pack[index++];
        vec4 lightDirection_cosOuterAngle = vsg_lights.pack[index++];
    }
#endif

    if (total_light_count > 0)
    {
        color.rgb = Lo + (clamp(ambient, vec3(0.0), vec3(1.0)) * albedo * pbr.ao);

//#if defined(ROCKY_ATMOSPHERE)
//        color.rgb += atmos_color; // add in the atmospheric haze
//        //color.rgb *= (atmos_color + vec3(1.0));
//#endif

        // option 1 - exposure mapping
        const float exposure = 3.3;
        color.rgb = 1.0 - exp(-exposure * color.rgb);

        // option 2 - reinhard tone mapping
        //color.rgb = color.rgb / (color.rgb + vec3(1.0)); // tone map

        // NOTE: no gamma correction needed, since VSG uses SRGB as the
        // default swapchain format. Ref:
        // https://github.com/vsg-dev/VulkanSceneGraph/discussions/1379
    }

    return color;
}
