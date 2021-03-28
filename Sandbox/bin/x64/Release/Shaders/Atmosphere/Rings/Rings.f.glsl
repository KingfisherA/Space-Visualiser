#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 vVertex;
in vec3 vLightVector;
//
out vec4 pixColour;

//Scene uniforms
uniform vec3 uLightPosition = vec3(0.0f);
uniform float uExposure = 5.0f;

//Ring Properties
uniform sampler2D uRingMap;
uniform float uRingWidth;

//Planet Properties
uniform vec3 uPlanetPosition = vec3(0.0f);
uniform float uPlanetRadius = 0.95f;
uniform float uInvShadowSoftness = 10.0f;

//Calculating shadow by ray intersection with sphere
float SoftSphereShadow(vec3 origin, vec3 direction, float radius2, float shadowSoftness)
{
    vec3 L = origin;
    float tca = dot(L, direction);
    float d2 = dot(L, L) - tca * tca;
    if (d2 > radius2)
    {
        return 1.0f;
    }
    float thc = sqrt(radius2 - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;
    if (t0 > t1 && t1 > 0.0f)
    {
        float val = t0;
        t0 = t1;
        t1 = val;
    }

    return (t0 < 0.0) ? 1.0 : smoothstep(1.0, 0.0, 2.5 * shadowSoftness * thc / t0);
}

//Vital Constansts
const float minPlanetShadowBrightness = 0.001f;
const float ambientLight = 0.0005f;

void main()
{
    vec3 centredVertex = uPlanetPosition - vVertex;
    float pixDistance = length(centredVertex);
    float radialTexCoordX = (pixDistance - 1.0f) / uRingWidth;

    vec3 unitNormal = gl_FrontFacing ? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, -1.0f, 0.0f);
    vec3 unitLightPos = normalize(uLightPosition);
    vec3 unitLightVector = normalize(vLightVector);

    float diffuseLighting = max(dot(unitNormal, unitLightPos), ambientLight);

    //Add planet shadowing
    diffuseLighting *= diffuseLighting > ambientLight ? max(SoftSphereShadow(centredVertex, unitLightVector,
                                                                             uPlanetRadius * uPlanetRadius,
                                                                             uInvShadowSoftness),
                                                            minPlanetShadowBrightness)
                                                      : 1.0f;

    vec4 diffuseColour = texture(uRingMap, vec2(radialTexCoordX, 0.25f));
    diffuseColour.rgb *= diffuseLighting;

    pixColour.rgb = pow(vec3(1.) - exp(-diffuseColour.rgb / vec3(1.0f) * uExposure), vec3(1. / 2.2));
    pixColour.a = pow(diffuseColour.a, 3.0f);
}
