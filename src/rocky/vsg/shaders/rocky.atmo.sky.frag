#version 450

// Physically-based atmospheric scattering fragment shader.
// Single-scattering ray march with Rayleigh, Mie, and ozone absorption.

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// Atmosphere parameters UBO
layout(set = 0, binding = 0) uniform SkyUniforms {
    vec2 ellipsoidAxes;
    float padding[2];
} atmo;

// Input varyings from vertex shader
layout(location = 0) in vec3 v_worldPos;
layout(location = 1) in vec3 v_cameraECEF;
layout(location = 2) in vec3 v_sunPosECEF;

// Output
layout(location = 0) out vec4 out_color;

#include "rocky.atmo.glsl"

// Number of samples along the view ray and light ray
#define VIEW_SAMPLES 16
#define LIGHT_SAMPLES 8

void main()
{
    vec3 cameraPos = v_cameraECEF;
    vec3 rayDir = normalize(v_worldPos - cameraPos);

    float cameraHeight = length(cameraPos);
    float rPlanet = min(atmo.ellipsoidAxes.x, atmo.ellipsoidAxes.y);
    float rAtmos = rPlanet + ATMO_THICKNESS;
    float exposure = ATMO_EXPOSURE;
    vec3 sunDir = normalize(v_sunPosECEF);

    // Intersect ray with atmosphere
    vec2 atmosHit = raySphereIntersect2(cameraPos, rayDir, rAtmos);

    // If ray misses the atmosphere entirely, discard
    if (atmosHit.y < 0.0)
        discard;

    // Determine the ray segment through the atmosphere
    float tNear = max(atmosHit.x, 0.0);  // clamp to camera if inside atmosphere
    float tFar = atmosHit.y;

    // Check if ray hits the planet -- if so, clip the far distance
    float planetHit = raySphereIntersect(cameraPos, rayDir, rPlanet);
    bool hitsGround = (planetHit > 0.0 && planetHit < tFar);
    if (hitsGround)
    {
        tFar = planetHit;
    }

    // Ray march through the atmosphere
    float stepLen = (tFar - tNear) / float(VIEW_SAMPLES);
    vec3 rayleighSum = vec3(0.0);
    vec3 mieSum = vec3(0.0);
    vec3 viewOpticalDepth = vec3(0.0);

    for (int i = 0; i < VIEW_SAMPLES; ++i)
    {
        float t = tNear + (float(i) + 0.5) * stepLen;
        vec3 samplePos = cameraPos + rayDir * t;
        float h = length(samplePos) - rPlanet;

        // Density at this sample
        float rhoR = rayleighDensity(h);
        float rhoM = mieDensity(h);
        float rhoO3 = ozoneDensity(h);

        // Local extinction
        vec3 localExt = BETA_RAYLEIGH * rhoR + BETA_MIE_EXT * rhoM + BETA_OZONE * rhoO3;

        // Accumulate view optical depth
        viewOpticalDepth += localExt * stepLen;
        vec3 viewTransmittance = exp(-viewOpticalDepth);

        // Sun transmittance: optical depth from sample point toward the sun.
        // If the sun ray hits the planet, this sample receives no direct light.
        float sunPlanetHit = raySphereIntersect(samplePos, sunDir, rPlanet);
        vec3 sunTransmittance;
        if (sunPlanetHit > 0.0)
        {
            sunTransmittance = vec3(0.0);
        }
        else
        {
            float sunPathLen = distToAtmo(samplePos, sunDir, rAtmos);
            float sunStepLen = sunPathLen / float(LIGHT_SAMPLES);
            vec3 sunOD = vec3(0.0);
            for (int j = 0; j < LIGHT_SAMPLES; ++j)
            {
                float ts = (float(j) + 0.5) * sunStepLen;
                vec3 sunSample = samplePos + sunDir * ts;
                float hs = length(sunSample) - rPlanet;
                float sRhoR = rayleighDensity(hs);
                float sRhoM = mieDensity(hs);
                float sRhoO3 = ozoneDensity(hs);
                sunOD += (BETA_RAYLEIGH * sRhoR + BETA_MIE_EXT * sRhoM + BETA_OZONE * sRhoO3) * sunStepLen;
            }
            sunTransmittance = exp(-sunOD);
        }

        // Accumulate in-scattered light
        rayleighSum += viewTransmittance * sunTransmittance * rhoR * stepLen;
        mieSum += viewTransmittance * sunTransmittance * rhoM * stepLen;
    }

    // Apply phase functions and scattering coefficients
    float cosTheta = dot(rayDir, sunDir);
    float phaseR = rayleighPhase(cosTheta);
    float phaseM = miePhase(cosTheta, MIE_G);

    vec3 scatteredLight = SUN_ILLUMINANCE * (BETA_RAYLEIGH * phaseR * rayleighSum + BETA_MIE_SCAT * phaseM * mieSum);

    // Add sun disk
    float sunCosAngle = cos(SUN_ANGULAR_RADIUS);
    if (cosTheta > sunCosAngle && !hitsGround)
    {
        // Limb darkening
        float sunAngle = acos(clamp(cosTheta, -1.0, 1.0));
        float normalizedR = sunAngle / SUN_ANGULAR_RADIUS;
        float limbDarkening = 1.0 - 0.6 * (1.0 - sqrt(max(0.0, 1.0 - normalizedR * normalizedR)));

        // Sun transmittance from camera through atmosphere
        vec3 sunTransmit;
        if (cameraHeight > rAtmos)
        {
            // Camera outside atmosphere: transmittance along ray from atmosphere entry
            float entryDist = max(atmosHit.x, 0.0);
            vec3 entryPos = cameraPos + rayDir * entryDist;
            float pathLen = distToAtmo(entryPos, rayDir, rAtmos);
            sunTransmit = transmittance(entryPos, rayDir, pathLen, rPlanet, LIGHT_SAMPLES);
        }
        else
        {
            sunTransmit = exp(-viewOpticalDepth);
        }

        float smoothEdge = smoothstep(sunCosAngle, sunCosAngle + 0.0002, cosTheta);
        scatteredLight += SUN_ILLUMINANCE * sunTransmit * limbDarkening * smoothEdge * 50.0;
    }

    // Tone mapping
    vec3 color = ACESFilmic(scatteredLight * exposure);

    // With additive blending, rays that hit the ground must output zero
    // so they contribute nothing on top of the terrain.
    if (hitsGround)
    {
        color = vec3(0.0);
    }

    out_color = vec4(color, 1.0);
}
