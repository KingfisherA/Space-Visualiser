#version 410

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

#include "./Shaders/Maths/Sphere Texture Coordinates.h.glsl"

//
in vec3 lightVector;
in vec3 normalVector;
in vec2 textureCoord;
//
out vec4 fragColour;
//
uniform vec3 lightColour;
uniform sampler2D diffuseMap;
//
const float ambientLight = 0.2f;

void main()
{
    vec4 textureCol = texture(diffuseMap, textureCoord);
    float diffuseAmount = max(dot(normalize(normalVector), normalize(lightVector)), ambientLight);
    vec3 diffuseColour = diffuseAmount * textureCol.rgb;
    fragColour = vec4(diffuseColour, textureCol.a);
}