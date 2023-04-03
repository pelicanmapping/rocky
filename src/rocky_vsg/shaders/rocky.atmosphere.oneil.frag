#version 450

// inputs from vertex shader
layout(location = 0) in vec3 atmos_v3Direction;
layout(location = 1) in vec3 atmos_mieColor;
layout(location = 2) in vec3 atmos_rayleighColor;
layout(location = 3) in vec3 atmos_lightDir;
layout(location = 4) in float atmos_renderFromSpace;

// fragment out
layout(location = 0) out vec4 out_color;

const float exposure = 3.5; // TODO

const float atmos_mie_g = -0.095;
const float atmos_mie_g2 = atmos_mie_g * atmos_mie_g;
const float atmos_fWeather = 1.0;

void main()
{
    float fCos = dot(atmos_lightDir, atmos_v3Direction) / length(atmos_v3Direction);
    float fRayleighPhase = 0.75 * (1.0 + fCos * fCos);  // 1.0
    float fMiePhase = 1.5 * ((1.0 - atmos_mie_g2) / (2.0 + atmos_mie_g2)) * (1.0 + fCos * fCos) / pow(1.0 + atmos_mie_g2 - 2.0 * atmos_mie_g * fCos, 1.5);
    vec3 f4Color = fRayleighPhase * atmos_rayleighColor + fMiePhase * atmos_mieColor;

    vec3 skyColor = 1.0 - exp(f4Color * -exposure);
    vec4 atmosColor;
    atmosColor.rgb = skyColor.rgb * atmos_fWeather;
    atmosColor.a = (skyColor.r + skyColor.g + skyColor.b) * 2.0;

    out_color = mix(atmosColor, vec4(f4Color, 1.0), atmos_renderFromSpace);
}
