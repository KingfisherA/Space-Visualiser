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
layout(set = 2, binding = 3) uniform sampler2D uRingMap;
layout(set = 2, binding = 4) uniform sampler2D uNightMap;

#define RING_MAP uRingMap
#define TRANSMITTANCE_TEXTURE transmittanceTexture
#include "Rings.glsl"

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

// Premultiplied: rgb is the light the planet and its atmosphere send to the camera, a is how much
// of what is behind them they hide
layout(location = 0) out vec4 pixColour;

float ApproximateLuminance(vec3 colour)
{
    return 0.2126 * colour.r + 0.7152 * colour.g + 0.0722 * colour.b;
}

// Distance along the ray to its first intersection with a sphere at the origin, if it hits it in
// front of the origin
bool IntersectSphere(vec3 origin, vec3 direction, float radius, out float distance)
{
    float b = dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - c;
    distance = -b - sqrt(max(discriminant, 0.0));
    return discriminant >= 0.0 && distance > 0.0;
}

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

// Earth: the original stylised shading, where the surface takes its brightness from the sky.
// Positions are in the atmosphere's sphere space (km). The maps are sampled at uv (the surface
// point's longitude and latitude, before the planet's turning), with the derivatives uvDx and uvDy.
vec4 ShadeEarthLike(vec3 camera, vec3 viewRay, vec3 skyPoint, vec3 sunDirection, vec3 toCamera, int flags, vec3 eclipse,
                    vec2 uv, vec2 uvDx, vec2 uvDy)
{
    AtmosphereParameters atmosphere = ATMOSPHERE;
    float time = uFrame.times.x;
    float externalAtmoIntensity = uPlanet.axes[0].w;
    vec3 unitNormal = normalize(vNormal);

    vec3 transmittance;
    vec3 radiance = ToneMap(GetSkyRadianceNew(atmosphere, transmittanceTexture, scatteringTexture, scatteringTexture,
                                              camera, viewRay, skyPoint, sunDirection, transmittance));
    float alpha = exp(ApproximateLuminance(radiance)) - 1.0;
    vec3 nightColour = vec3(0.0);

    float groundDistance;
    if (IntersectSphere(camera, viewRay, atmosphere.bottom_radius, groundDistance))
    {
        vec2 offsetCoords = uv + vec2(time * uPlanet.shape.w, 0.0);
        vec3 surfaceMapColour = textureGrad(uDiffuseMap, offsetCoords, uvDx, uvDy).rgb * 0.5;

        if ((flags & PLANET_HAS_SPECULAR) != 0)
        {
            const float damper = 80.0;
            float specReflectivity = textureGrad(uSpecularMap, offsetCoords, uvDx, uvDy).r * 8.0;
            SpecularPhong(specReflectivity, unitNormal, toCamera, sunDirection, vec3(0.1), damper, surfaceMapColour);
        }

        float dotResult = dot(unitNormal, sunDirection);
        vec3 diffuseColour = dotResult * vec3(1.3);

        if ((flags & PLANET_HAS_CLOUDS) != 0)
        {
            const float cloudSpeed = 0.5;
            vec2 cloudTexCoords = uv + vec2(time, 0.0) * cloudSpeed;

            const float shadowOffset = 0.015;
            const float shadowDarkness = 0.02;
            vec4 cloudsMapColour = textureGrad(uCloudsMap, cloudTexCoords, uvDx, uvDy);
            float cloudShadowOffset = -((1.0 - dotResult) * shadowOffset);

            vec4 cloudsShadowColour = textureGrad(uCloudsMap, cloudTexCoords + vec2(cloudShadowOffset), uvDx, uvDy);
            // Cloud shadows
            surfaceMapColour *= max(1.0 - cloudsShadowColour.r, shadowDarkness);
            // Clouds
            surfaceMapColour *= 1.0 - cloudsMapColour.r;
            surfaceMapColour += cloudsMapColour.rgb * cloudsMapColour.r * diffuseColour;
        }

        float luminance = clamp(abs(ApproximateLuminance(radiance)), 0.0, 1.0);
        surfaceMapColour *= luminance;

        radiance = radiance * externalAtmoIntensity + surfaceMapColour;

        if ((flags & PLANET_HAS_NIGHT) != 0)
        {
            const float spreadFactor = 6.0;
            const float offsetFactor = 0.13;
            float invLuminance = smoothstep(0.0, 1.0, max(1.0 - (dotResult + offsetFactor) * spreadFactor, 0.0));
            nightColour = textureGrad(uNightMap, offsetCoords, uvDx, uvDy).rgb * invLuminance;
        }

        alpha = 1.0;
    }

    // A moon's shadow darkens the sky and the surface, but not the city lights
    return vec4(radiance * eclipse + nightColour, alpha);
}

// The fraction of the drawn disc the body covers: below 1 when a body smaller than a pixel is drawn
// inflated (see PlanetRenderer::Render)
float coverage = 1.0;

void ShadePlanet()
{
    AtmosphereParameters atmosphere = ATMOSPHERE;
    int flags = int(uPlanet.axes[1].w + 0.5);
    float polarRatio = uPlanet.shape.x;

    // Planet space: centred on the planet, y along its axis, in equatorial radii
    mat3 toPlanet = transpose(GetPlanetRotation());
    vec3 centre = uPlanet.transform[3].xyz;
    float unitsPerRadius = uPlanet.axes[2].w;
    vec3 camera = toPlanet * (uFrame.cameraPosition.xyz - centre) / unitsPerRadius;
    vec3 skyPoint = toPlanet * (vWorldPosition - centre) / unitsPerRadius;

    // A body drawn inflated is shaded as its true disc, scaled up to the drawn one
    float inflation = length(uPlanet.transform[0].xyz) / (unitsPerRadius * atmosphere.top_radius / atmosphere.bottom_radius);
    if (inflation > 1.001)
    {
        skyPoint /= inflation;
        coverage = 1.0 / (inflation * inflation);
    }
    vec3 sunOffset = uFrame.sun.xyz - centre;
    vec3 toSun = normalize(toPlanet * sunOffset);
    float sunAngularRadius = asin(min(uFrame.sun.w / length(sunOffset), 1.0));

    // The spherical atmosphere is evaluated in sphere space (the spheroid squashed back to a
    // sphere), in the atmosphere's length unit
    vec3 cameraS = ToSphereSpace(camera, polarRatio) * atmosphere.bottom_radius;
    vec3 skyPointS = ToSphereSpace(skyPoint, polarRatio) * atmosphere.bottom_radius;
    vec3 viewRayS = normalize(skyPointS - cameraS);
    vec3 sunDirectionS = normalize(ToSphereSpace(toSun, polarRatio));

    // Eclipses: the sunlight reaching the point below this one on the surface, past the moons and
    // planets that can cast shadows here
    vec3 surfaceBelow = normalize(ToSphereSpace(skyPoint, polarRatio));
    vec3 eclipse = SunVisibility(centre + GetPlanetRotation() * FromSphereSpace(surfaceBelow, polarRatio) * unitsPerRadius, centre);

    // The surface point the view ray reaches (or, for rays that miss, the nearest point to it)
    float groundDistance;
    bool hitsGround = IntersectSphere(cameraS, viewRayS, atmosphere.bottom_radius, groundDistance);
    vec3 groundS = hitsGround ? cameraS + viewRayS * groundDistance
                              : normalize(cameraS - viewRayS * dot(cameraS, viewRayS)) * atmosphere.bottom_radius;
    vec3 ground = FromSphereSpace(groundS / atmosphere.bottom_radius, polarRatio);

    // Map coordinates of the surface point, worked out for each pixel. The sky mesh's own are offset
    // by parallax near the limb, and are interpolated across the triangles that straddle the map's
    // date line from near 1 back to near 0, squeezing the whole map into a seam there. Derivatives are
    // taken here, outside any branch, with the longitude wrap removed (so the seam's pixels don't
    // sample the smallest mip level).
    vec3 groundDirection = normalize(ground);
    vec2 uv = vec2((atan(groundDirection.x, groundDirection.z) + PI) / (2.0 * PI),
                   acos(clamp(groundDirection.y, -1.0, 1.0)) / PI);
    vec2 uvDx = dFdx(uv);
    vec2 uvDy = dFdy(uv);
    uvDx.x -= round(uvDx.x);
    uvDy.x -= round(uvDy.x);

    if ((flags & PLANET_LIT_SURFACE) == 0)
    {
        vec4 colour = ShadeEarthLike(cameraS, viewRayS, skyPointS, sunDirectionS, normalize(camera - skyPoint), flags, eclipse,
                                     uv, uvDx, uvDy);
        float alpha = clamp(colour.a, 0.0, 1.0);
        pixColour = vec4(colour.rgb * alpha, alpha); // Premultiplied
        return;
    }
    uv.x += uFrame.times.x * uPlanet.shape.w;

    // The rings' shadow, and other bodies'. It falls on the air above the surface as well as on the
    // surface, so it also dims the light the atmosphere scatters (the shadow is taken at the surface
    // point below).
    vec3 shadow = vec3(1.0);
    if ((flags & PLANET_HAS_RINGS) != 0)
        shadow = RingShadow(ground, toSun, uPlanet.shape.y, uPlanet.shape.z, sunAngularRadius);
    // Shadows of other bodies (taken at the surface point, like the rings')
    shadow *= SunVisibility(centre + GetPlanetRotation() * ground * unitsPerRadius, centre);

    if (!hitsGround)
    {
        // Sky: the limb of the atmosphere, which glows when lit from behind (forward scattering)
        vec3 transmittance;
        vec3 radiance = ToneMap(shadow * GetSkyRadiance(atmosphere, transmittanceTexture, scatteringTexture,
                                                            scatteringTexture, cameraS, viewRayS, 0.0, sunDirectionS,
                                                            transmittance));
        // The air adds its light and hides what is behind it by its opacity along the ray (faint
        // glows stay visible, unlike with an opacity derived from their brightness)
        pixColour = vec4(radiance, 1.0 - dot(transmittance, vec3(1.0 / 3.0)));
        return;
    }

    // Light scattered into the view between the camera and the surface, and the surface's
    // transmittance to the camera
    vec3 viewTransmittance;
    vec3 inScattered = GetSkyRadianceToPoint(atmosphere, transmittanceTexture, scatteringTexture, scatteringTexture,
                                             cameraS, groundS, 0.0, sunDirectionS, viewTransmittance);

    // Sunlight reaching the surface through the atmosphere (past the rings and other bodies)
    vec3 normal = normalize(vec3(ground.x, ground.y / (polarRatio * polarRatio), ground.z));
    float r = length(groundS);
    vec3 sunlight = atmosphere.solar_irradiance *
                    GetTransmittanceToSun(atmosphere, transmittanceTexture, r, dot(groundS, sunDirectionS) / r) *
                    max(dot(normal, toSun), 0.0) * shadow;
    vec3 ringshine = vec3(0.0);
    if ((flags & PLANET_HAS_RINGS) != 0)
    {
        float innerRadius = uPlanet.shape.y;
        float outerRadius = uPlanet.shape.z;
        ringshine = atmosphere.solar_irradiance *
                    RingshineIrradiance(ground, normal, toSun, innerRadius, outerRadius, polarRatio, sunAngularRadius);
    }

    // The diffuse map is a display image normalised to its brightest point: scale it to the
    // planet's reflectance
    vec3 albedo = pow(textureGrad(uDiffuseMap, uv, uvDx, uvDy).rgb, vec3(2.2)) * uPlanet.axes[0].w;
    vec3 surface = albedo / PI * (sunlight + ringshine);

    pixColour = vec4(ToneMap(surface * viewTransmittance + inScattered * shadow), 1.0);
}

void main()
{
    ShadePlanet();
    pixColour *= coverage; // Premultiplied
}
