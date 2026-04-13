// rocky.atmo.ground.frag.glsl
// Computes atmospheric inscattering and transmittance between
// the camera and the terrain surface using an analytic approximation.

#include "rocky.atmo.glsl"

// Apply aerial perspective to a lit terrain color.
// color: the lit terrain color (linear HDR)
// vertex_VS: vertex position in view space
// vertex_ECEF: vertex position in ECEF world space
// cameraECEF: camera position in ECEF world space (from vertex shader varying)
// sunDirECEF: normalized sun direction in ECEF
// Returns the color with aerial perspective applied.
vec3 apply_atmo_color_to_ground(
    vec3 color,
    vec3 vertex_VS,
    vec3 vertex_ECEF,
    vec3 cameraECEF,
    vec3 sunDirECEF,
    vec2 ellipsoidAxes)
{
    float rPlanet = min(ellipsoidAxes.x, ellipsoidAxes.y);
    float rAtmos = rPlanet + ATMO_THICKNESS;

    float dist = length(vertex_VS); // camera-to-vertex distance
    vec3 viewDir = normalize(vertex_ECEF - cameraECEF);

    // Altitudes
    float h_camera = length(cameraECEF) - rPlanet;
    float h_surface = length(vertex_ECEF) - rPlanet;
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
    vec3 midpoint = cameraECEF + viewDir * dist * 0.5;
    float sunPathLen = distToAtmo(midpoint, sunDirECEF, rAtmos);
    vec3 sunTransmitMid = transmittance(midpoint, sunDirECEF, sunPathLen, rPlanet, 4);

    float cosTheta = dot(viewDir, sunDirECEF);
    float phaseR = rayleighPhase(cosTheta);
    float phaseM = miePhase(cosTheta, MIE_G);

    vec3 scatterCoeff = BETA_RAYLEIGH * rhoR * phaseR + BETA_MIE_SCAT * rhoM * phaseM;

    // Inscattered light: integral approximation using the analytic form
    // integral of scatter * exp(-ext*t) dt from 0 to dist = scatter * (1 - exp(-ext*dist)) / ext
    vec3 safeExt = max(avgExt, vec3(1e-10));
    vec3 inscatter = SUN_ILLUMINANCE * scatterCoeff * sunTransmitMid * (1.0 - viewTransmittance) / safeExt;

    return color * viewTransmittance + inscatter;
}
