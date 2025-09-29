#version 450

// inter-stage interface block
struct Varyings {
    vec4 color;
    vec2 stippleDir;
    int stipplePattern;
    int stippleFactor;
};
layout(location = 1) in float lateral;
layout(location = 2) flat in Varyings vary;

// outputs
layout(location = 0) out vec4 out_color;


void main()
{
    out_color = vary.color;

    if (vary.color.a == 0.0)
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
        int ci = (cx % (16 * vary.stippleFactor)) / vary.stippleFactor;
        int pattern16 = 0xffff & (vary.stipplePattern & (1 << ci));
        if (pattern16 == 0)
            discard;
    }

    //anti-aliasing (requires blending state be set)
    float L = abs(lateral);
    out_color.a *= smoothstep(0.0, 1.0, 1.0 - (L * L));
}