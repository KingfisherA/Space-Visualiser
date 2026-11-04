#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(push_constant) uniform MarkerPushConstants
{
    mat4 transform;
    vec4 colour; // rgb, a = opacity
} uMarker;

layout(location = 0) out vec3 vLightVector;
layout(location = 1) out vec3 vNormal;

void main()
{
    vec4 positionWorld = uMarker.transform * vec4(aPosition, 1.0);
    vNormal = mat3(uMarker.transform) * aNormal;
    vLightVector = uFrame.sun.xyz - positionWorld.xyz;
    gl_Position = uFrame.viewProjection * positionWorld;
}
