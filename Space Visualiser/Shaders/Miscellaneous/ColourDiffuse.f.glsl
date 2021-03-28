#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 lightVector;
in vec3 normalVector;
in vec2 textureCoord;
//
out vec4 fragColour;
//
uniform vec3 lightColour = vec3(1.0f);
uniform vec4 diffuseColour;
uniform float ambientLight = 0.2f;
//

void main()
{
    float diffuseAmount = max(dot(normalize(normalVector), normalize(lightVector)), ambientLight);
    fragColour = vec4(diffuseAmount * diffuseColour.rgb * lightColour, diffuseColour.a);
}