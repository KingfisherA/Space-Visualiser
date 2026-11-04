#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"

layout(location = 0) in vec3 vPosition;

layout(location = 0) out vec4 fragColour;

void main()
{
    const float density = 1E-2;
    const float gradient = 5.0;

    float distance = length(uFrame.cameraCentre.xz - vPosition.xz);
    fragColour = vec4(1.0, 1.0, 1.0, exp(-pow(distance * density, gradient)));
}
