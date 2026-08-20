#version 450
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

// inter-stage interface block
struct Varyings {
    vec4 color;
    vec4 outlineColor;
    float outlineRatio;
    vec2 stippleDir;
    int stipplePattern;
    int stippleFactor;
};
layout(location = 1) in float lateral;
layout(location = 2) flat in Varyings vary;

// outputs
layout(location = 0) out vec4 outColor;


void main()
{
    float L = abs(lateral);
    outColor = vary.color;

    if (vary.outlineRatio < 1.0)
    {
        // Blend across approximately one fragment at the core/outline edge.
        float aa = max(fwidth(L), 1e-6);
        float outlineMix = smoothstep(
            vary.outlineRatio - aa,
            vary.outlineRatio + aa,
            L);
        outColor = mix(vary.color, vary.outlineColor, outlineMix);
    }

    if (outColor.a == 0.0)
    {
        discard; // signal to not draw a segment
    }

    if (vary.stipplePattern != 0xffff)
    {
        // coordinate of the fragment, shifted to 0:
        vec2 coord = (gl_FragCoord.xy - 0.5);

        // rotate the frag coord onto the X-axis to sample the stipple pattern linearly
        // note: the mat2 inverts the y coordinate (sin(angle)) because we want the 
        // rotation angle to be the negative of the stipple direction angle.
        vec2 rv = normalize(vary.stippleDir);
        vec2 coord_proj = mat2(rv.x, -rv.y, rv.y, rv.x) * coord;

        // sample the stippling pattern (16-bits repeating)
        int cx = int(coord_proj.x);
        int period = 16 * max(1, vary.stippleFactor);
        int wrapped = ((cx % period) + period) % period;
        int ci = wrapped / max(1, vary.stippleFactor);
        int pattern16 = 0xffff & (vary.stipplePattern & (1 << ci));
        if (pattern16 == 0)
            discard;
    }

    //anti-aliasing (requires blending state be set)
    outColor.a *= smoothstep(0.0, 1.0, 1.0 - (L * L));
}
