#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 fPos;
//
out vec4 fragColour;
//
uniform vec3 camPosition = vec3(0.0f);
//

void main()
{
    fragColour = vec4(1.0f);

    float density = 1E-2;
    float gradient = 5.0f;

    float distance = length(camPosition.xz - fPos.xz);
    float vis = exp(-pow((distance * density), gradient));
    fragColour.a = vis;
}