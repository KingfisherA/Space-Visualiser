#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "GridPushConstants.glsl"

layout(location = 0) in vec3 aPosition;

layout(location = 0) out vec3 vPosition;

void main()
{
    vec4 positionWorld = uGrid.transform * vec4(aPosition, 1.0);
    vPosition = positionWorld.xyz;
    gl_Position = uFrame.viewProjection * positionWorld;
}
