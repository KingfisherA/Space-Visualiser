#version 410
#extension GL_ARB_shading_language_420pack : require

layout(push_constant) uniform MarkerPushConstants
{
    mat4 transform;
    vec4 colour;
} uMarker;

layout(location = 0) in vec3 vLightVector;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 fragColour;

const float AMBIENT_LIGHT = 0.2;

void main()
{
    float diffuseAmount = max(dot(normalize(vNormal), normalize(vLightVector)), AMBIENT_LIGHT);
    fragColour = vec4(diffuseAmount * uMarker.colour.rgb, uMarker.colour.a);
}
