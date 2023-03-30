#version 450
#extension GL_NV_fragment_shader_barycentric : enable

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

// inputs
layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_upvector_view;
layout(location = 3) in vec2 normalmap_coords;
layout(location = 4) in vec3 normalmap_binormal;
layout(location = 5) in vec3 in_vertex_view;

// uniforms
layout(set = 0, binding = 11) uniform sampler2D color_tex;
layout(set = 0, binding = 12) uniform sampler2D normal_tex;

// from VSG's view-dependent state
layout(set = 1, binding = 0) uniform LightData {
    vec4 values[64];
} lightData;

// outputs

layout(location = 0) out vec4 out_color;

void apply_lighting(inout vec4 color);

void main()
{
    vec4 texel = texture(color_tex, in_uv);
    out_color = mix(vec4(in_color,1), clamp(texel, 0, 1), texel.a);

    if (gl_FrontFacing == false)
        out_color.r = 1.0;

    apply_lighting(out_color);

    // outlines - debugging
    float b = min(gl_BaryCoordNV.x, min(gl_BaryCoordNV.y, gl_BaryCoordNV.z))*32.0;
    out_color.rgb = mix(vec3(1,1,1), out_color.rgb, clamp(b,0.85,1.0));
}



struct PBR {
    float roughness;
    float metal;
    float ao;
} pbr;

vec3 get_normal()
{
#if 0
    return in_upvector_view;
#else
    // temporary! until we support normal maps
    vec3 dx = dFdx(in_vertex_view);
    vec3 dy = dFdy(in_vertex_view);
    vec3 n = -normalize(cross(dx, dy));
    return n;
#endif
}

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
    float k = (r*r) / 8.0;
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

void apply_lighting(inout vec4 color)
{
// temp:
    pbr.ao = 1.0;
    pbr.roughness = 0.0;
    pbr.metal = 0.0;
    const float exposure = 3.3;
// ....

    vec3 albedo = color.rgb;
    vec3 N = normalize(get_normal());
    vec3 V = normalize(-in_vertex_view);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, vec3(pbr.metal));

    vec3 Lo = vec3(0.0);
    vec3 ambient = vec3(0.003);

    vec4 light_counts = lightData.values[0];
    int ambient_count = int(light_counts[0]);
    int directional_count = int(light_counts[1]);
    int point_count = int(light_counts[2]);
    int spot_count = int(light_counts[3]);
    int total_light_count = ambient_count + directional_count + point_count + spot_count;
    int index = 1;

    for(int i = 0; i<ambient_count; ++i)
    {
        vec4 diffuse = lightData.values[index++];
        ambient += diffuse.rgb * diffuse.a;
    }

    for(int i = 0; i<directional_count; ++i)
    {
        vec3 diffuse = lightData.values[index++].rgb;
        vec3 direction = normalize(lightData.values[index++].xyz);
    }

    for(int i = 0; i<point_count; ++i)
    {
        vec3 diffuse = lightData.values[index++].rgb;
        vec3 position = lightData.values[index++].xyz;
        
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
    for(int i = 0; i<spot_count; ++i)
    {
        vec4 light_color = lightData.values[index++];
        vec4 position_cosInnerAngle = lightData.values[index++];
        vec4 lightDirection_cosOuterAngle = lightData.values[index++];
    }
    #endif

    if (total_light_count > 0)
    {
        color.rgb = Lo + (ambient * albedo * pbr.ao);
    
        color.rgb = color.rgb / (color.rgb + vec3(1.0)); // tone map
        color.rgb = pow(color.rgb, vec3(1.0/2.2)); // gamma correction
        //color.rgb += sky_color; // haze
        color.rgb = 1.0 - exp(-exposure * color.rgb); // exposure
    }
}
