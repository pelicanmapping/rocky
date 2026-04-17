// rocky.atmo.glsl
// Shared physically-based atmosphere rendering library.
// Implements single-scattering with Rayleigh, Mie, and ozone absorption.
// Based on Bruneton 2017 scattering coefficients.

#ifndef ROCKY_PI
#define ROCKY_PI
const float PI = 3.14159265359;
#endif

//const vec3 betaR = vec3(5.802e-6, 13.558e-6, 33.1e-6); // rayleigh scattering coefficients for red, green, blue (m^-1)
//const float hR = 8500.0; // scale height for rayleigh scattering (m)
//const vec3 betaM = vec3(3.996e-6f, 3.996e-6, 3.996e-6); // mie scattering coefficients (m^-1)
//const float hM = 1200.0; // scale height for mie scattering (m)
//const vec3 betaMext = vec3(4.44e-6, 4.44e-6, 4.44e-6); // mie extinction coefficients (scattering + absorption)
//const vec3 betaO3 = vec3(0.65e-6, 1.881e-6, 0.085e-6); // ozone absorption coefficients (m^-1)
//const float o3Center = 25000.0; // center altitude of ozone layer (m)
//const float o3Width = 15000.0; // width of ozone layer (m)
//const float mieG = 0.76; // mie phase function anisotropy factor (0.76 is typical for Earth's atmosphere)
//const float sunIllum = 20.0; // approximate solar illuminance at sea level (W/m^2/nm)
//const float atmoThickness = 100000.0; // max distance to sample atmosphere (m)

const float ATMO_THICKNESS = 100000.0; // max distance to sample atmosphere (m)
const float ATMO_EXPOSURE = 10.0; // exposure multiplier for atmospheric effects (tune for artistic preference)

// Planet and atmosphere geometry
//const float R_PLANET = 6371000.0;       // mean Earth radius (m)
//const float R_ATMOS  = 6471000.0;       // top of atmosphere (R_PLANET + 100km)

// Scale heights
const float H_RAYLEIGH = 8500.0;        // Rayleigh scale height (m)
const float H_MIE      = 1200.0;        // Mie scale height (m)

// Scattering/absorption coefficients at sea level (per meter)
const vec3 BETA_RAYLEIGH = vec3(5.802e-6, 13.558e-6, 33.1e-6);
const vec3 BETA_MIE_SCAT = vec3(3.996e-6);
const vec3 BETA_MIE_EXT  = BETA_MIE_SCAT / 0.9;  // single-scattering albedo = 0.9
const vec3 BETA_OZONE    = vec3(0.650e-6, 1.881e-6, 0.085e-6);

// Ozone layer parameters
const float OZONE_CENTER = 25000.0;     // center altitude (m)
const float OZONE_WIDTH  = 15000.0;     // half-width (m)

// Mie asymmetry parameter
const float MIE_G = 0.76;

// Sun angular radius (radians) ~0.536 degrees
const float SUN_ANGULAR_RADIUS = 0.00935;

// Sun disk illuminance (pre-atmosphere, neutral white)
const vec3 SUN_ILLUMINANCE = vec3(20.0);

// -------------------------------------------------------------------
// Ray-sphere intersection
// Returns distance to nearest intersection, or -1 if no intersection
// -------------------------------------------------------------------
float raySphereIntersect(vec3 origin, vec3 dir, float radius)
{
    float b = dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float d = b * b - c;
    if (d < 0.0) return -1.0;
    d = sqrt(d);
    float t0 = -b - d;
    float t1 = -b + d;
    if (t0 > 0.0) return t0;
    if (t1 > 0.0) return t1;
    return -1.0;
}

// Returns (near, far) intersection distances. near may be negative (inside sphere).
vec2 raySphereIntersect2(vec3 origin, vec3 dir, float radius)
{
    float b = dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float d = b * b - c;
    if (d < 0.0) return vec2(-1.0);
    d = sqrt(d);
    return vec2(-b - d, -b + d);
}

// -------------------------------------------------------------------
// Phase functions
// -------------------------------------------------------------------
float rayleighPhase(float cosTheta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

// Cornette-Shanks Mie phase function
float miePhase(float cosTheta, float g)
{
    float g2 = g * g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
    float denom = (8.0 * PI) * (2.0 + g2) * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
    return num / denom;
}

// -------------------------------------------------------------------
// Density functions
// -------------------------------------------------------------------
float rayleighDensity(float altitude)
{
    return exp(-altitude / H_RAYLEIGH);
}

float mieDensity(float altitude)
{
    return exp(-altitude / H_MIE);
}

float ozoneDensity(float altitude)
{
    return max(0.0, 1.0 - abs(altitude - OZONE_CENTER) / OZONE_WIDTH);
}

// Combined extinction coefficient at a given altitude
vec3 extinction(float altitude)
{
    float rho_r = rayleighDensity(altitude);
    float rho_m = mieDensity(altitude);
    float rho_o = ozoneDensity(altitude);
    return BETA_RAYLEIGH * rho_r + BETA_MIE_EXT * rho_m + BETA_OZONE * rho_o;
}

// -------------------------------------------------------------------
// Optical depth integration along a ray segment
// -------------------------------------------------------------------
vec3 opticalDepth(vec3 origin, vec3 dir, float pathLength, float rPlanet, int numSamples)
{
    float stepLen = pathLength / float(numSamples);
    vec3 od = vec3(0.0);
    for (int i = 0; i < numSamples; ++i)
    {
        float t = (float(i) + 0.5) * stepLen;
        vec3 p = origin + dir * t;
        float h = length(p) - rPlanet;
        od += extinction(h) * stepLen;
    }
    return od;
}

// Transmittance along a ray segment
vec3 transmittance(vec3 origin, vec3 dir, float pathLength, float rPlanet, int numSamples)
{
    return exp(-opticalDepth(origin, dir, pathLength, rPlanet, numSamples));
}

// Distance from a point to the top of the atmosphere along a direction
float distToAtmo(vec3 origin, vec3 dir, float rAtmos)
{
    vec2 t = raySphereIntersect2(origin, dir, rAtmos);
    return max(t.y, 0.0);
}

// -------------------------------------------------------------------
// ACES filmic tone mapping
// -------------------------------------------------------------------
vec3 ACESFilmic(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// -------------------------------------------------------------------
// Camera ECEF extraction from modelview matrix
// -------------------------------------------------------------------
vec3 cameraPositionECEF(mat4 modelview)
{
    // The camera position in world space is -inverse(mat3(MV)) * MV[3].xyz
    // For an orthonormal rotation (no scale), inverse(mat3) == transpose(mat3)
    mat3 rot = mat3(modelview);
    vec3 t = modelview[3].xyz;
    return -transpose(rot) * t;
}
