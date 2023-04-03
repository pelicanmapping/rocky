
// from VSG's view-dependent state
layout(set = 1, binding = 0) uniform LightData {
    vec4 v[64];
} vsg_lights;

layout(location = 15) in vec3 atmos_color;

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

void apply_lighting(inout vec4 color, in vec3 normal)
{
    // temp:
    pbr.ao = 1.0;
    pbr.roughness = 0.5;
    pbr.metal = 0.0;
    const float exposure = 3.3;
    // ....

    vec3 albedo = pow(color.rgb, vec3(2.2)); // SRGB to linear

    vec3 N = normalize(normal);
    vec3 V = normalize(-in_vertex_view);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, vec3(pbr.metal));

    vec3 Lo = vec3(0.0);
    vec3 ambient = vec3(0.013);

    vec4 light_counts = vsg_lights.v[0];
    int ambient_count = int(light_counts[0]);
    int directional_count = int(light_counts[1]);
    int point_count = int(light_counts[2]);
    int spot_count = int(light_counts[3]);
    int total_light_count = ambient_count + directional_count + point_count + spot_count;
    int index = 1;

    for (int i = 0; i < ambient_count; ++i)
    {
        vec4 diffuse = vsg_lights.v[index++];
        ambient += diffuse.rgb * diffuse.a;
    }

    for (int i = 0; i < directional_count; ++i)
    {
        vec3 diffuse = vsg_lights.v[index++].rgb;
        vec3 direction = normalize(vsg_lights.v[index++].xyz);
    }

    for (int i = 0; i < point_count; ++i)
    {
        vec3 diffuse = vsg_lights.v[index++].rgb;
        vec3 position = vsg_lights.v[index++].xyz;

        // per-light radiance:
        vec3 L = normalize(position - in_vertex_view);
        vec3 H = normalize(V + L);
        vec3 radiance = diffuse;

        // cook-torrance BRDF:
        float NDF = DistributionGGX(N, H, pbr.roughness);
        float G = GeometrySmith(N, V, L, pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NdotL = max(dot(N, L), 0.0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL;
        vec3 specular = numerator / max(denominator, 0.001);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - pbr.metal;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

#if 0
    for (int i = 0; i < spot_count; ++i)
    {
        vec4 light_color = vsg_lights.values[index++];
        vec4 position_cosInnerAngle = vsg_lights.values[index++];
        vec4 lightDirection_cosOuterAngle = vsg_lights.values[index++];
    }
#endif

    if (total_light_count > 0)
    {
        color.rgb = Lo + (ambient * albedo * pbr.ao);

        color.rgb = color.rgb / (color.rgb + vec3(1.0)); // tone map

        color.rgb += atmos_color; // add in the (linear) atmospheric haze

        color.rgb = 1.0 - exp(-exposure * color.rgb); // exposure

        // linear to SRGB (last step)
        color.rgb = pow(color.rgb, vec3(1.0 / 2.2));

        //color.rgb = clamp(color.rgb, 0, 1);
    }
}
