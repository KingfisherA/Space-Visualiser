#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(push_constant) uniform StarPushConstants
{
    mat4 transform;
    vec4 properties; // radius, temperature, noise octaves
    vec4 colour;
} uStar;

layout(location = 0) out vec3 vVertex;
layout(location = 1) out vec3 vCamVector;
layout(location = 2) out vec3 vNormal;

void main()
{
    vec4 positionWorld = uStar.transform * vec4(aPosition, 1.0);
    vVertex = aPosition;
    // Stars are uniformly scaled, so the model matrix keeps normal directions
    vNormal = mat3(uStar.transform) * aNormal;
    vCamVector = uFrame.cameraPosition.xyz - positionWorld.xyz;
    gl_Position = uFrame.viewProjection * positionWorld;
}
