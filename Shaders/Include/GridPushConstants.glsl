#ifndef SV_GRID_PUSH_CONSTANTS_GLSL
#define SV_GRID_PUSH_CONSTANTS_GLSL

// Mirrors SV::GridPushConstants
layout(push_constant) uniform GridPushConstants
{
    mat4 transform; // The unit grid mesh to world (one scale of the grid, around the camera's centre)
    vec4 params;    // x = distance from the camera's centre at which the lines fade out, y = opacity
} uGrid;

#endif // SV_GRID_PUSH_CONSTANTS_GLSL
