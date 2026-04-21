// rocky.atmo.ground.frag.glsl
// Computes atmospheric inscattering and transmittance between
// the camera and the terrain surface using an analytic approximation.

#include "rocky.atmo.glsl"

// Apply aerial perspective to a lit terrain color.
// color: the lit terrain color (linear HDR)
// vertex_vs: vertex position in view space
// vertex_ecef: vertex position in ECEF world space
// camera_ecef: camera position in ECEF world space (from vertex shader varying)
// sunDir_ecef: normalized sun direction in ECEF
// Returns the color with aerial perspective applied.
vec3 apply_atmo_color_to_ground(
    vec3 color,
    vec3 vertex_vs,
    vec3 vertex_ecef,
    vec3 camera_ecef,
    vec3 sunDir_ecef,
    vec2 ellipsoidAxes)
{
    float lat = abs(dot(normalize(vertex_ecef), vec3(0, 0, 1)));
    float rPlanet = mix(ellipsoidAxes.x, ellipsoidAxes.y, lat);
    float rAtmos = rPlanet + ATMO_THICKNESS;

    float dist = length(vertex_vs); // camera-to-vertex distance
    vec3 viewDir = normalize(vertex_ecef - camera_ecef);

    // Altitudes
    float h_camera = length(camera_ecef) - rPlanet;
    float h_surface = length(vertex_ecef) - rPlanet;
    float h_avg = max(0.5 * (h_camera + h_surface), 0.0);

    // Density at average altitude
    float rhoR = rayleighDensity(h_avg);
    float rhoM = mieDensity(h_avg);
    float rhoO3 = ozoneDensity(h_avg);

    // Combined extinction at average altitude
    vec3 avgExt = BETA_RAYLEIGH * rhoR + BETA_MIE_EXT * rhoM + BETA_OZONE * rhoO3;

    // Transmittance along the view path
    vec3 viewTransmittance = exp(-avgExt * dist);

    // Inscattering approximation at the midpoint
    vec3 midpoint = camera_ecef + viewDir * dist * 0.5;
    float sunPathLen = distToAtmo(midpoint, sunDir_ecef, rAtmos);
    vec3 sunTransmitMid = transmittance(midpoint, sunDir_ecef, sunPathLen, rPlanet, 4);

    float cosTheta = dot(viewDir, sunDir_ecef);
    float phaseR = rayleighPhase(cosTheta);
    float phaseM = miePhase(cosTheta, MIE_G);

    vec3 scatterCoeff = BETA_RAYLEIGH * rhoR * phaseR + BETA_MIE_SCAT * rhoM * phaseM;

    // Inscattered light: integral approximation using the analytic form
    // integral of scatter * exp(-ext*t) dt from 0 to dist = scatter * (1 - exp(-ext*dist)) / ext
    vec3 safeExt = max(avgExt, vec3(1e-10));
    vec3 inscatter = SUN_ILLUMINANCE * scatterCoeff * sunTransmitMid * (1.0 - viewTransmittance) / safeExt;

    return color * viewTransmittance + inscatter;
}
