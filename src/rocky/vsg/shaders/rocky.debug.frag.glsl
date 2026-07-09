/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#extension GL_EXT_fragment_shader_barycentric : enable
#pragma import_defines(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION)


void applyDebugTriangles(inout vec4 outColor, float strength)
{
#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    // wireframe overlay
    vec3 b = fwidth(gl_BaryCoordEXT.xyz);
    vec3 edge = smoothstep(vec3(0.0), b, gl_BaryCoordEXT.xyz);
    float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
    vec3 wire_color = clamp(outColor.rgb * 3.0, 0.05, 1.0);
    outColor.rgb = mix(outColor.rgb, wire_color, clamp(wire, 0.0, 1.0) * strength);
#endif
}
