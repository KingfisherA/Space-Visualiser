#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "RingsPushConstants.glsl"

layout(set = 1, binding = 0) uniform sampler2D uRingMap;

layout(location = 0) in vec3 vVertex;

layout(location = 0) out vec4 pixColour;

// Shadow cast by the planet, from a ray-sphere intersection
float SoftSphereShadow(vec3 origin, vec3 direction, float radius2, float shadowSoftness)
{
    vec3 L = origin;
    float tca = dot(L, direction);
    float d2 = dot(L, L) - tca * tca;
    if (d2 > radius2)
    {
        return 1.0;
    }
    float thc = sqrt(radius2 - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;
    if (t0 > t1 && t1 > 0.0)
    {
        float val = t0;
        t0 = t1;
        t1 = val;
    }

    return (t0 < 0.0) ? 1.0 : smoothstep(1.0, 0.0, 2.5 * shadowSoftness * thc / t0);
}

const float MIN_PLANET_SHADOW_BRIGHTNESS = 0.001;
const float AMBIENT_LIGHT = 0.0005;

void main()
{
    vec3 lightPosition = uFrame.sun.xyz;
    float exposure = uFrame.sun.w;
    float ringWidth = uRings.properties.x;
    float planetRadius = uRings.properties.y;
    float invShadowSoftness = uRings.properties.z;

    vec3 centredVertex = uRings.transform[3].xyz - vVertex;
    float pixDistance = length(centredVertex);
    float radialTexCoordX = (pixDistance - 1.0) / ringWidth;

    vec3 unitNormal = gl_FrontFacing ? vec3(0.0, 1.0, 0.0) : vec3(0.0, -1.0, 0.0);
    vec3 unitLight = normalize(lightPosition);

    float diffuseLighting = max(dot(unitNormal, unitLight), AMBIENT_LIGHT);

    // Planet shadowing
    diffuseLighting *= diffuseLighting > AMBIENT_LIGHT
                         ? max(SoftSphereShadow(centredVertex, unitLight, planetRadius * planetRadius, invShadowSoftness),
                               MIN_PLANET_SHADOW_BRIGHTNESS)
                         : 1.0;

    vec4 diffuseColour = texture(uRingMap, vec2(radialTexCoordX, 0.25));
    diffuseColour.rgb *= diffuseLighting;

    pixColour.rgb = pow(vec3(1.0) - exp(-diffuseColour.rgb * exposure), vec3(1.0 / 2.2));
    pixColour.a = pow(diffuseColour.a, 3.0);
}
