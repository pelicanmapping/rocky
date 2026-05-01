#pragma import_defines(ROCKY_ATMOSPHERE)

#pragma import_defines(VSG_SHADOWS_HARD)
#if defined(VSG_SHADOWS_HARD) || defined(VSG_SHADOWS_SOFT) || defined(VSG_SHADOWS_PCSS)
#define ROCKY_SHADOWS
#endif

#ifndef VIEW_DESCRIPTOR_SET
#define VIEW_DESCRIPTOR_SET 1
#endif

// from VSG's view-dependent state
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform VSGLightData {
    vec4 values[64];
} lightData;

// shadows.glsl is copied, unmodified, from vsgExamples
vec3 eyePos; // expected by shadows.glsl
#pragma include "shadows.glsl"

// placeholder
struct PBR {
    float roughness;
    float metal;
    float ao;
} pbr;

#ifndef ROCKY_PI
#define ROCKY_PI
const float PI = 3.14159265359;
#endif

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

// ACES filmic tone mapping (shared with atmosphere shaders)
vec3 ACES_tonemap(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 get_sunlight_direction()
{
    int index = 0;
    vec4 light_counts = lightData.values[0];
    int ambient_count = int(light_counts[0]); // 1 component each (color)
    int sun = 1 + (ambient_count * 1);
    return lightData.values[sun + 1].xyz; // first directional light direction
}

vec4 apply_lighting(in vec4 color, in vec3 vertex_vs, in vec3 normal_vs)
{
    eyePos = vertex_vs;

    // temp:
    pbr.ao = 1.0;
    pbr.roughness = 0.95;
    pbr.metal = 0.0;

    vec3 albedo = color.rgb;

    vec3 N = normalize(normal_vs);
    vec3 V = normalize(-vertex_vs);

    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, vec3(pbr.metal));

    vec3 Lo = vec3(0.0);
    vec3 ambient = vec3(0.0);

    vec4 light_counts = lightData.values[0];
    int ambient_count = int(light_counts[0]);
    int directional_count = int(light_counts[1]);
    int point_count = int(light_counts[2]);
    int spot_count = int(light_counts[3]);
    int total_light_count = ambient_count + directional_count + point_count + spot_count;
    int index = 1;

    for (int i = 0; i < ambient_count; ++i)
    {
        vec4 light_color = lightData.values[index++];
        ambient += light_color.rgb * light_color.a;
    }

    int shadowmapIndex = 0;

    for (int i = 0; i < directional_count; ++i)
    {
        vec3 light_color = lightData.values[index].rgb;
        float intensity = lightData.values[index++].a;
        vec3 direction = lightData.values[index++].xyz;

        int shadowmapCount = int(lightData.values[index].r);
        if (shadowmapCount > 0)
        {
#ifdef ROCKY_SHADOWS
            intensity *= (1.0 - calculateShadowCoverageForDirectionalLight(index, shadowmapIndex, vec3(0), vec3(0), Lo));
#endif
            index += 1 + 8 * shadowmapCount;
            shadowmapIndex += shadowmapCount;
        }
        else
        {
            index++;
        }

        // per-light radiance:
        vec3 L = normalize(-direction);
        vec3 H = normalize(V + L);
        vec3 radiance = light_color * intensity;

        // cook-torrance BRDF:
        float D = DistributionGGX(N, H, pbr.roughness);
        float G = GeometrySmith(N, V, L, pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NdotL = max(0.0, dot(N, L));

        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * max(NdotL, 0.001) + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - pbr.metal;

        vec3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    for (int i = 0; i < point_count; ++i)
    {
        vec3 light_color = lightData.values[index++].rgb;
        vec3 position_vs = lightData.values[index++].xyz;

        // per-light radiance:
        vec3 L = normalize(position_vs - vertex_vs);
        vec3 H = normalize(V + L);
        vec3 radiance = light_color;

        // cook-torrance BRDF:
        float D = DistributionGGX(N, H, pbr.roughness);
        float G = GeometrySmith(N, V, L, pbr.roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float NdotL = max(0.0, dot(N, L));

        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * max(NdotL, 0.001) + 0.001;
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
        vec4 light_color = lightData.values[index++];
        vec4 position_cosInnerAngle = lightData.values[index++];
        vec4 lightDirection_cosOuterAngle = lightData.values[index++];
        int shadowmap_count = lightData.values[index].r;
        if (shadowmap_count > 0)
        {
            // todo: process shadowmaps
            index += 1 + 8 * shadowmap_count;
        }
        else
        {
            index++;
        }
    }
#endif

    vec3 new_color = Lo + (clamp(ambient, vec3(0.0), vec3(1.0)) * albedo * pbr.ao);

    // Tone mapping
    new_color = ACES_tonemap(new_color * 3.3);

    float apply = min(1.0, float(total_light_count));
    color.rgb = mix(color.rgb, new_color, apply);

    return color;
}
