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
    vec4 sun;            // xyz = the Sun's position in world space, w = the Sun's radius
    vec4 times;          // x = planet animation time, y = star animation time
    vec4 display;        // x = exposure
} uFrame;

// Maps scene radiance to display colour
vec3 ToneMap(vec3 radiance)
{
    return pow(vec3(1.0) - exp(-radiance * uFrame.display.x), vec3(1.0 / 2.2));
}

#endif // SV_FRAME_GLSL
