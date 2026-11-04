#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "PlanetPushConstants.glsl"

// Set 1: the planet's atmosphere
#define ATMOSPHERE_SET 1
#define ATMOSPHERE_BINDING 0
#include "Atmosphere/Parameters.glsl"
layout(set = 1, binding = 1) uniform sampler2D transmittanceTexture;
layout(set = 1, binding = 2) uniform sampler3D scatteringTexture;

// Set 2: surface maps. Optional maps are flagged in uPlanet (a black texture is bound otherwise).
layout(set = 2, binding = 0) uniform sampler2D uDiffuseMap;
layout(set = 2, binding = 1) uniform sampler2D uSpecularMap;
layout(set = 2, binding = 2) uniform sampler2D uCloudsMap;
layout(set = 2, binding = 3) uniform sampler2D uRingsShadowMap;
layout(set = 2, binding = 4) uniform sampler2D uNightMap;

layout(location = 0) in vec3 vVertex;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;
layout(location = 3) in vec3 vToCamVector;
layout(location = 4) in vec2 vRingShadowParams;

layout(location = 0) out vec4 pixColour;

const vec3 WHITE_POINT = vec3(1.0);

void SpecularPhong(float specReflectivity, vec3 unitNormal, vec3 unitToCameraVector, vec3 unitSunVector,
                   vec3 lightColour, float shineDamper, inout vec3 surfaceMapColour)
{
    if (specReflectivity > 0.0)
    {
        vec3 reflectedLightVector = reflect(-unitSunVector, unitNormal);
        float specularFactor = max(dot(reflectedLightVector, unitToCameraVector), 0.0);
        surfaceMapColour += pow(specularFactor, shineDamper) * specReflectivity * lightColour;
    }
}

float ApproximateLuminance(vec3 colour)
{
    return 0.2126 * colour.r + 0.7152 * colour.g + 0.0722 * colour.b;
}

// Sky radiance in front of the surface. The scattering texture is combined, so it is passed
// for the single Mie scattering texture too (which is then unused).
vec3 GetSkyRadianceNew(vec3 camera, vec3 viewRay, vec3 vPos, vec3 sunDirection, out vec3 transmittance)
{
    return GetSkyRadianceNew(ATMOSPHERE, transmittanceTexture, scatteringTexture, scatteringTexture,
                             camera, viewRay, vPos, sunDirection, transmittance);
}

void main()
{
    AtmosphereParameters atmosphere = ATMOSPHERE;
    vec3 planetCentre = uPlanet.transform[3].xyz;
    float externalAtmoIntensity = uPlanet.normalMatrix[0].w;
    int mapFlags = int(uPlanet.normalMatrix[1].w + 0.5);
    float exposure = uFrame.sun.w;
    float time = uFrame.times.x;

    vec3 unitSunVector = normalize(uFrame.sun.xyz);
    vec3 unitNormal = normalize(vNormal);
    vec3 scaledVertex = vVertex * atmosphere.bottom_radius; // Must not have any translation
    vec3 scaledCamera = (uFrame.cameraPosition.xyz - planetCentre) * atmosphere.bottom_radius;
    vec3 viewDirection = normalize(scaledVertex - scaledCamera);

    vec3 scaledCamTop = (uFrame.cameraPosition.xyz - planetCentre) * atmosphere.top_radius;
    vec3 scaledVertexTop = vVertex * atmosphere.top_radius;

    float tca = dot(scaledCamTop, viewDirection);
    float thc = sqrt(atmosphere.bottom_radius * atmosphere.bottom_radius - (dot(scaledCamTop, scaledCamTop) - tca * tca));

    vec3 transmittance;
    vec3 radiance = GetSkyRadianceNew(scaledCamTop, viewDirection, scaledVertexTop, unitSunVector, transmittance);

    radiance = pow(vec3(1.0) - exp(-radiance / WHITE_POINT * exposure), vec3(1.0 / 2.2));

    pixColour.a = exp(ApproximateLuminance(radiance)) - 1.0;
    vec3 nightColour = vec3(0.0);

    // Test for sphere intersection
    if (tca - thc < 0.0 && tca + thc < 0.0)
    {
        const float rotationSpeed = 0.3;
        vec2 offsetCoords = vTexCoord + vec2(time, 0.0) * rotationSpeed;
        vec3 surfaceMapColour = texture(uDiffuseMap, offsetCoords).rgb * 0.5;

        if ((mapFlags & PLANET_HAS_SPECULAR) != 0)
        {
            const float damper = 80.0;
            float specReflectivity = texture(uSpecularMap, offsetCoords).r * 8.0;
            SpecularPhong(specReflectivity, unitNormal, normalize(vToCamVector), unitSunVector, vec3(0.1),
                          damper, surfaceMapColour);
        }

        float dotResult = dot(unitNormal, unitSunVector);
        vec3 diffuseColour = dotResult * vec3(1.3);

        if ((mapFlags & PLANET_HAS_CLOUDS) != 0)
        {
            const float cloudSpeed = 0.5;
            vec2 cloudTexCoords = vTexCoord + vec2(time, 0.0) * cloudSpeed;

            const float shadowOffset = 0.015;
            const float shadowDarkness = 0.02;
            vec4 cloudsMapColour = texture(uCloudsMap, cloudTexCoords);
            float cloudShadowOffset = -((1.0 - dotResult) * shadowOffset);

            vec4 cloudsShadowColour = texture(uCloudsMap, cloudTexCoords + vec2(cloudShadowOffset));
            // Cloud shadows
            surfaceMapColour *= max(1.0 - cloudsShadowColour.r, shadowDarkness);
            // Clouds
            surfaceMapColour *= 1.0 - cloudsMapColour.r;
            surfaceMapColour += cloudsMapColour.rgb * cloudsMapColour.r * diffuseColour;
        }

        float luminance = clamp(abs(ApproximateLuminance(radiance)), 0.0, 1.0);
        surfaceMapColour *= luminance;

        radiance = radiance * externalAtmoIntensity + surfaceMapColour;

        if ((mapFlags & PLANET_HAS_RING_SHADOW) != 0)
        {
            float ringsShadow = 1.0 - vRingShadowParams.y * texture(uRingsShadowMap, vec2(vRingShadowParams.x, 0.75)).a;
            radiance *= ringsShadow;
        }

        if ((mapFlags & PLANET_HAS_NIGHT) != 0)
        {
            const float spreadFactor = 6.0;
            const float offsetFactor = 0.13;
            float invLuminance = smoothstep(0.0, 1.0, max(1.0 - (dotResult + offsetFactor) * spreadFactor, 0.0));
            nightColour = texture(uNightMap, offsetCoords).rgb * invLuminance;
        }

        pixColour.a = 1.0;
    }

    pixColour.rgb = radiance + nightColour;
}
