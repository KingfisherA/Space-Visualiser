#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"

layout(location = 0) in vec2 aPosition;

layout(push_constant) uniform CoronaPushConstants
{
    mat4 transform;
    vec4 colour;
} uCorona;

layout(location = 0) out vec3 vVertex;

const float NOISE_SCALE = 1.5;

void main()
{
    vVertex = vec3(aPosition * NOISE_SCALE, 0.0);

    // Billboard: span the quad along the camera's right and up axes
    mat4 view = uFrame.view;
    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 positionModel = cameraRight * vVertex.x + cameraUp * vVertex.y;

    gl_Position = uFrame.viewProjection * uCorona.transform * vec4(positionModel, 1.0);
}
