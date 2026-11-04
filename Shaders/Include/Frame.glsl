#ifndef SV_FRAME_GLSL
#define SV_FRAME_GLSL

// Per-frame scene values shared by every scene shader (set 0). Mirrors SV::FrameUniforms (std140).
layout(std140, set = 0, binding = 0) uniform FrameUniforms
{
    mat4 viewProjection;
    mat4 view;
    mat4 projection;
    vec4 cameraPosition; // xyz = camera position in world space
    vec4 cameraCentre;   // xyz = the point the camera orbits
    vec4 sun;            // xyz = light position, w = exposure
    vec4 times;          // x = planet animation time, y = star animation time
} uFrame;

#endif // SV_FRAME_GLSL
