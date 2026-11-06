#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "CoronaPushConstants.glsl"

layout(location = 0) in vec2 aPosition;

layout(location = 0) out vec3 vWorldPosition;

void main()
{
    // Billboard through the star's centre, spanning the camera's right and up axes
    mat4 view = uFrame.view;
    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 centre = uCorona.transform[3].xyz;
    float halfSize = length(uCorona.transform[0].xyz) * CORONA_EXTENT;
    vWorldPosition = centre + (cameraRight * aPosition.x + cameraUp * aPosition.y) * halfSize;
    gl_Position = uFrame.viewProjection * vec4(vWorldPosition, 1.0);
}
