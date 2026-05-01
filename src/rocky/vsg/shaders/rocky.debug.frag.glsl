#extension GL_EXT_fragment_shader_barycentric : enable
#pragma import_defines(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION)


void apply_debug_triangles(inout vec4 out_color, float strength)
{
#if defined(ROCKY_HAS_VK_BARYCENTRIC_EXTENSION) && defined(GL_EXT_fragment_shader_barycentric)
    // wireframe overlay
    vec3 b = fwidth(gl_BaryCoordEXT.xyz);
    vec3 edge = smoothstep(vec3(0.0), b, gl_BaryCoordEXT.xyz);
    float wire = 1.0 - min(min(edge.x, edge.y), edge.z);
    vec3 wire_color = clamp(out_color.rgb * 3.0, 0.05, 1.0);
    out_color.rgb = mix(out_color.rgb, wire_color, clamp(wire, 0.0, 1.0) * strength);
#endif
}
