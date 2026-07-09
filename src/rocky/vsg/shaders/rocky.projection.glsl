/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

// the camera position in model space (or tile tangent space)
//vec3 getCameraMs()
//{
//    return -transpose(mat3(pc.modelview)) * pc.modelview[3].xyz;
//}

// get a matrix that will rotate a view-space vector into world-space
mat3 getRotateVsToWs(in mat3 modelMatrix, in mat4 mvm)
{
    return modelMatrix * transpose(mat3(mvm));
}

// get a matrix that will rotate a view-space vector into world-space
mat3 getRotateVsToWs(in mat4 mvm)
{
    return mat3(u_vds.inverseViewMatrix * mvm) * transpose(mat3(mvm));
}

// get the parametric distance along ta ray at which it intersects an ellipsoid
// (t, such that isect = origin + t * dir)
float rayEllipsoidIntersect(in vec3 origin, in vec3 dir, in vec2 axes)
{
    vec3 invAxes2 = 1.0 / vec3(axes.x * axes.x, axes.x * axes.x, axes.y * axes.y);
    float a = dot(dir * dir, invAxes2);
    float b = 2.0 * dot(origin * dir, invAxes2);
    float c = dot(origin * origin, invAxes2) - 1.0;
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant <= 0.0 || a <= 0.0)
        return -1.0;

    float root = sqrt(discriminant);
    float t = (-b - root) / (2.0 * a);
    if (t <= 0.0)
        t = (-b + root) / (2.0 * a);

    return t;
}

// unit normal vector to the ellipsoid at a point
vec3 ellipsoidNormal(in vec3 point, in vec2 axes)
{
    return normalize(vec3(
        point.x / (axes.x * axes.x),
        point.y / (axes.x * axes.x),
        point.z / (axes.y * axes.y)));
}

vec3 projectToSphericalGnomonic(in vec3 position_vs, in float R, in mat4 viewMatrix)
{
    vec3 sphereCenter_vs = (viewMatrix * vec4(0, 0, 0, 1)).xyz;
    vec3 look_vs = vec3(0, 0, -1);
    vec3 planeNormal_vs = vec3(0, 0, 1);
    vec3 radius_vs = position_vs - sphereCenter_vs;
    float distance = length(radius_vs);

    if (distance <= 1e-6)
        return position_vs;

    vec3 surface_position_vs = sphereCenter_vs + radius_vs * (R / distance);
    float height = distance - R;

    float b = dot(sphereCenter_vs, look_vs);
    float discriminant = b * b - (dot(sphereCenter_vs, sphereCenter_vs) - R * R);

    if (discriminant <= 0.0)
        return surface_position_vs + height * planeNormal_vs;

    float root = sqrt(discriminant);
    float t = b - root;
    if (t <= 0.0)
        t = b + root;
    if (t <= 0.0)
        return surface_position_vs + height * planeNormal_vs;

    vec3 planeOrigin_vs = look_vs * t;
    vec3 dir_vs = surface_position_vs - sphereCenter_vs;
    float denom = dot(dir_vs, planeNormal_vs);

    if (denom <= 1e-6)
        return surface_position_vs + height * planeNormal_vs;

    float scale = dot(planeOrigin_vs - sphereCenter_vs, planeNormal_vs) / denom;
    return sphereCenter_vs + dir_vs * scale + height * planeNormal_vs;
}

vec3 projectVertexToStereographic(in vec3 position_vs, in vec2 ellipsoid, in mat4 viewMatrix)
{
    vec3 sphereCenter_vs = (viewMatrix * vec4(0, 0, 0, 1)).xyz;
    vec3 look_vs = vec3(0, 0, -1);
    vec3 planeNormal_vs = vec3(0, 0, 1);

    vec3 northpoledir_vs = mat3(transpose(viewMatrix)) * vec3(0, 0, 1);
    float lat = clamp(abs(dot(look_vs, northpoledir_vs)), 0.0, 1.0);
    float R = mix(ellipsoid.x, ellipsoid.y, lat);
    vec3 radius_vs = position_vs - sphereCenter_vs;
    float distance = length(radius_vs);

    if (distance <= 1e-6)
        return position_vs;

    vec3 surface_position_vs = sphereCenter_vs + radius_vs * (R / distance);
    float height = distance - R;

    float b = dot(sphereCenter_vs, look_vs);
    float discriminant = b * b - (dot(sphereCenter_vs, sphereCenter_vs) - R * R);

    if (discriminant <= 0.0)
        return surface_position_vs + height * planeNormal_vs;

    float root = sqrt(discriminant);
    float t = b - root;
    if (t <= 0.0)
        t = b + root;
    if (t <= 0.0)
        return surface_position_vs + height * planeNormal_vs;

    vec3 planeOrigin_vs = look_vs * t;
    vec3 antipode_vs = sphereCenter_vs - (planeOrigin_vs - sphereCenter_vs);
    vec3 dir_vs = surface_position_vs - antipode_vs;
    float denom = dot(dir_vs, planeNormal_vs);

    if (denom <= 1e-6)
        return surface_position_vs + height * planeNormal_vs;

    float scale = dot(planeOrigin_vs - antipode_vs, planeNormal_vs) / denom;
    return antipode_vs + dir_vs * scale + height * planeNormal_vs;
}

vec3 projectAnchoredVertexToStereographic(in vec3 position_vs, in vec2 ellipsoid, in mat4 viewMatrix, in mat4 mvm)
{
    vec3 anchor_vs = mvm[3].xyz;
    vec3 offset_vs = position_vs - anchor_vs;
    vec3 projected_anchor_vs = projectVertexToStereographic(anchor_vs, ellipsoid, viewMatrix);
    return projected_anchor_vs + offset_vs;
}

vec4 applyProjection(in vec4 position_vs, in mat4 proj)
{
    if (u_vds.stereographic > 0 && proj[3][3] > 0.0)
    {
        mat4 viewMatrix = inverse(u_vds.inverseViewMatrix);
        position_vs.xyz = projectVertexToStereographic(position_vs.xyz, u_vds.ellipsoidAxes, viewMatrix);
    }
    return position_vs;
}
