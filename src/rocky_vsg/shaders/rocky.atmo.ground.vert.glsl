// from VSG's view-dependent state
layout(set = 1, binding = 0) uniform LightData {
    vec4 v[64];
} vsg_lights;

// The output:
layout(location = 15) out vec3 atmos_color;

const float polarRadius = 6356752.0;
const float atmos_Thickness = 96560.0;
const float atmos_fInnerRadius = polarRadius;
const float atmos_fOuterRadius = polarRadius + atmos_Thickness;

const float PI = 3.1415927;
const float Kr = 0.0025;
const float Km = 0.0015;
const float ESun = 15.0;
const float RaleighScaleDepth = 0.25;
const float atmos_fKrESun = Kr * ESun;
const float atmos_fKmESun = Km * ESun;
const float atmos_fKr4PI = Kr * 4 * PI;
const float atmos_fKm4PI = Km * 4 * PI;
const vec3  atmos_v3InvWavelength = vec3(5.6020447, 9.4732844, 19.6438026);
#define N_SAMPLES 2
#define F_SAMPLES 2.0

// locals
float atmos_fCameraHeight;
float atmos_fCameraHeight2;
float atmos_fOuterRadius2;
float atmos_fScale;
float atmos_fScaleOverScaleDepth;

vec3 atmos_lightDir;       // light direction in view space
vec3 earth_center;

float atmos_scale(float fCos)
{
    float x = 1.0 - fCos;
    return RaleighScaleDepth * exp(-0.00287 + x * (0.459 + x * (3.83 + x * (-6.80 + x * 5.25))));
}

void atmos_GroundFromSpace(in vec3 vertex_view)
{
    vec3 v3Pos = vertex_view;
    vec3 v3Ray = v3Pos;
    float fFar = length(v3Ray);
    v3Ray /= fFar;
    vec3 normal = normalize(v3Pos - earth_center);

    // Calculate the closest intersection of the ray with the outer atmosphere 
    // (which is the near point of the ray passing through the atmosphere) 
    float B = 2.0 * dot(-earth_center, v3Ray);
    float C = atmos_fCameraHeight2 - atmos_fOuterRadius2;
    float fDet = max(0.0, B * B - 4.0 * C);
    float fNear = 0.5 * (-B - sqrt(fDet));

    // Calculate the ray's starting position, then calculate its scattering offset 
    vec3 v3Start = v3Ray * fNear;
    fFar -= fNear;
    //float fDepth = exp((atmos_fInnerRadius - atmos_fOuterRadius) / RaleighScaleDepth);
    float fDepth = exp(-atmos_Thickness / RaleighScaleDepth);
    float fCameraAngle = dot(-v3Ray, normal);  // try max(0, ...) to get rid of yellowing building tops
    float fLightAngle = dot(atmos_lightDir, normal);
    float fCameraScale = atmos_scale(fCameraAngle);
    float fLightScale = atmos_scale(fLightAngle);
    float fCameraOffset = fDepth * fCameraScale;
    float fTemp = fLightScale * fCameraScale;

    // Initialize the scattering loop variables 
    float fSampleLength = fFar / F_SAMPLES;
    float fScaledLength = fSampleLength * atmos_fScale;
    vec3 v3SampleRay = v3Ray * fSampleLength;
    vec3 v3SamplePoint = v3Start + v3SampleRay * 0.5;

    // Now loop through the sample rays 
    vec3 v3FrontColor = vec3(0.0, 0.0, 0.0);
    vec3 v3Attenuate = vec3(1, 0, 0);

    for (int i = 0; i < N_SAMPLES; ++i)
    {
        float fHeight = length(v3SamplePoint - earth_center);
        float fDepth = exp(atmos_fScaleOverScaleDepth * (atmos_fInnerRadius - fHeight));
        float fScatter = fDepth * fTemp - fCameraOffset;
        v3Attenuate = exp(-fScatter * (atmos_v3InvWavelength * atmos_fKr4PI + atmos_fKm4PI));
        v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
        v3SamplePoint += v3SampleRay;
    }

    atmos_color = v3FrontColor * (atmos_v3InvWavelength * atmos_fKrESun + atmos_fKmESun);
}

void atmos_GroundFromAtmosphere(in vec3 vertex_view)
{
    // Get the ray from the camera to the vertex and its length (which is the far point of the ray passing through the atmosphere) 
    vec3 v3Pos = vertex_view.xyz;
    vec3 v3Ray = v3Pos;
    float fFar = length(v3Ray);
    v3Ray /= fFar;
    vec3 normal = normalize(v3Pos - earth_center);

    // Calculate the ray's starting position, then calculate its scattering offset 
    float fDepth = exp((atmos_fInnerRadius - atmos_fCameraHeight) / RaleighScaleDepth);
    float fCameraAngle = max(0.0, dot(-v3Ray, normal));
    float fLightAngle = dot(atmos_lightDir, normal);
    float fCameraScale = atmos_scale(fCameraAngle);
    float fLightScale = atmos_scale(fLightAngle);
    float fCameraOffset = fDepth * fCameraScale;
    float fTemp = fLightScale * fCameraScale;

    // Initialize the scattering loop variables 	
    float fSampleLength = fFar / F_SAMPLES;
    float fScaledLength = fSampleLength * atmos_fScale;
    vec3 v3SampleRay = v3Ray * fSampleLength;
    vec3 v3SamplePoint = v3SampleRay * 0.5;

    // Now loop through the sample rays 
    vec3 v3FrontColor = vec3(0.0, 0.0, 0.0);
    vec3 v3Attenuate;
    for (int i = 0; i < N_SAMPLES; i++)
    {
        float fHeight = length(v3SamplePoint - earth_center);
        float fDepth = exp(atmos_fScaleOverScaleDepth * (atmos_fInnerRadius - fHeight));
        float fScatter = fDepth * fTemp - fCameraOffset;
        v3Attenuate = exp(-fScatter * (atmos_v3InvWavelength * atmos_fKr4PI + atmos_fKm4PI));
        v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
        v3SamplePoint += v3SampleRay;
    }

    atmos_color = v3FrontColor * (atmos_v3InvWavelength * atmos_fKrESun + atmos_fKmESun);
}

void atmos_vertex_main(inout vec3 vertex_view)
{
    // Get camera position and height (from tile space)
    earth_center = (pc.modelview * inverse(tile.model_matrix) * vec4(0, 0, 0, 1)).xyz;
    atmos_fCameraHeight = length(earth_center);
    atmos_fCameraHeight2 = atmos_fCameraHeight * atmos_fCameraHeight;

    atmos_fOuterRadius2 = atmos_fOuterRadius * atmos_fOuterRadius;
    atmos_fScale = 1.0 / (atmos_fOuterRadius - atmos_fInnerRadius);
    atmos_fScaleOverScaleDepth = atmos_fScale / RaleighScaleDepth;

    // calculate the light direction in view space:
    vec4 light_counts = vsg_lights.v[0];
    int sun = 1 + (int(light_counts[0]) * 1) + (int(light_counts[1]) * 2); // first point light
    // vec4 diffuse = vsg_light.v[sun + 0];
    vec4 light_pos_view = vsg_lights.v[sun + 1];
    atmos_lightDir = normalize(light_pos_view.xyz);

    if (atmos_fCameraHeight >= atmos_fOuterRadius)
    {
        atmos_GroundFromSpace(vertex_view);
    }
    else
    {
        atmos_GroundFromAtmosphere(vertex_view);
    }

    // SRGB to linear
    atmos_color = pow(atmos_color, vec3(2.2));
}
