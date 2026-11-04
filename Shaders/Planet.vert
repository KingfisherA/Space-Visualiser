#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "PlanetPushConstants.glsl"

layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 vVertex;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;
layout(location = 3) out vec3 vToCamVector;
layout(location = 4) out vec2 vRingShadowParams; // (ring shadow x texture coordinate, ring shadow fade)

const float BOTTOM_RADIUS = 6360.0;
const vec3 RING_PARAMS = vec3(1.0 / BOTTOM_RADIUS, BOTTOM_RADIUS + 300.0, 1.0);

void CalculateRingShadow(vec3 lightPos, vec3 vertexIn, vec3 ringParamsIn, out vec2 ringShadowParamsOut)
{
    vec3 lightPosA = lightPos - vertexIn.xyz;
    vec2 shadowProj = vertexIn.xz - lightPosA.xz * min(vertexIn.y / lightPosA.y, 0.0);
    float ringShadowCoord = (length(shadowProj) - ringParamsIn.y) * ringParamsIn.x;
    float sina = abs(lightPos.y) * inversesqrt(dot(lightPos, lightPos));
    float ringShadowFade = ringParamsIn.z * smoothstep(0.0003, 0.001, sina);

    ringShadowParamsOut = vec2(ringShadowCoord, ringShadowFade);
}

void main()
{
    vec4 positionWorld = uPlanet.transform * vec4(aVertex, 1.0);
    mat3 normalMatrix = GetPlanetNormalMatrix();

    vVertex = normalMatrix * aVertex; // Must not have any translation
    vNormal = normalMatrix * aNormal;
    vTexCoord = aTexCoord;
    vToCamVector = uFrame.cameraPosition.xyz - positionWorld.xyz;

    vec3 planetCentre = uPlanet.transform[3].xyz;
    vec3 transformedSunPos = ((-normalize(uFrame.sun.xyz) + vec3(0.0, 0.2, 0.0)) * BOTTOM_RADIUS) + planetCentre;
    CalculateRingShadow(transformedSunPos, aVertex * BOTTOM_RADIUS, RING_PARAMS, vRingShadowParams);

    gl_Position = uFrame.viewProjection * positionWorld;
}
