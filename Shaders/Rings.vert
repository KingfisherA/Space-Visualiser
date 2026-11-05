#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "RingsPushConstants.glsl"

layout(location = 0) in vec2 aPosition;

layout(location = 0) out vec3 vWorldPosition;

void main()
{
    vec4 positionWorld = uRings.transform * vec4(aPosition, 0.0, 1.0);
    vWorldPosition = positionWorld.xyz;
    gl_Position = uFrame.viewProjection * positionWorld;
}
