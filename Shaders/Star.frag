#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Maths/NoiseFunction.glsl"
#include "TemperatureShift.glsl"

layout(push_constant) uniform StarPushConstants
{
    mat4 transform;
    vec4 properties; // radius, temperature, noise octaves
    vec4 colour;
} uStar;

layout(location = 0) in vec3 vVertex;
layout(location = 1) in vec3 vCamVector;
layout(location = 2) in vec3 vNormal;

layout(location = 0) out vec4 fragColour;

void main()
{
    vec3 colour = uStar.colour.rgb;

    float radius = uStar.properties.x;
    float starTemp = uStar.properties.y;
    int noiseOctaves = int(ceil(uStar.properties.z));

    if (noiseOctaves == 0)
    {
        colour += ColourTemperatureShift(starTemp);
    }
    else
    {
        // Granule noise
        vec4 position = vec4(vVertex, uFrame.times.y);
        float granuleNoise = (noise(position, noiseOctaves, 40.0, 0.7) + 1.0) * 0.5;

        // Sunspot noise
        const float sunSpotIntensity = 0.4;
        const float sunSpotFrequency = 0.75;
        vec4 sPosition = position * radius;
        float t1 = snoise(sPosition * sunSpotFrequency) - sunSpotIntensity;
        float t2 = snoise((sPosition + radius) * sunSpotFrequency) - sunSpotIntensity;
        float sunSpotNoise = (max(t1, 0.0) * max(t2, 0.0)) * 2.0;

        colour += ColourTemperatureShift(starTemp);
        colour *= 0.2 + (granuleNoise - sunSpotNoise);
    }

    colour *= 0.75;

    // Depth lighting from the angle between the camera and this fragment
    float theta = (dot(normalize(vNormal), normalize(vCamVector)) + 1.0) * 0.5;
    colour *= theta;

    fragColour = vec4(colour, 1.0);
}
