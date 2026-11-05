#version 410
#extension GL_ARB_shading_language_420pack : require

#include "FlarePushConstants.glsl"

layout(location = 0) in vec2 aPosition;

// This fragment's position in view units
layout(location = 0) out vec2 vPosition;

void main()
{
    // The quad covers the whole viewport
    vPosition = vec2(aPosition.x * uFlare.star.w, aPosition.y);
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
