#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"

layout(location = 0) in vec3 aPosition;

layout(push_constant) uniform GridPushConstants
{
    mat4 transform;
} uGrid;

layout(location = 0) out vec3 vPosition;

const float SEPARATION_DISTANCE = 10.0;

void main()
{
    vec4 positionWorld = uGrid.transform * vec4(aPosition, 1.0);
    // The grid follows the camera's orbit centre
    positionWorld.xyz += uFrame.cameraCentre.xyz - SEPARATION_DISTANCE;
    vPosition = positionWorld.xyz;
    gl_Position = uFrame.viewProjection * positionWorld;
}
