#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Maths/NoiseFunction.glsl"
#include "Maths/Utilities.glsl"

layout(push_constant) uniform CoronaPushConstants
{
    mat4 transform;
    vec4 colour;
} uCorona;

layout(location = 0) in vec3 vVertex;

layout(location = 0) out vec4 fragColour;

const int NOISE_OCTAVES = 5;
const float INTENSITY = 3.0;
const float INV_INTENSITY = 1.0 / (INTENSITY * 1.3);

void main()
{
    float time = uFrame.times.y;
    float t = time - length(vVertex);

    // Offset normal with noise
    const float frequency = 1.5;
    float ox = snoise(vec4(vVertex, t) * frequency);
    float oy = snoise(vec4((vVertex + 2000.0), t) * frequency);
    float oz = snoise(vec4((vVertex + 4000.0), t) * frequency);

    vec3 offsetVec = vec3(ox, oy, oz) * 0.1;
    vec3 distanceFromCentre = normalize(vVertex + offsetVec);

    vec3 positionNoise = vVertex + noise(vec4(distanceFromCentre, time), NOISE_OCTAVES, 2.0, 0.7) * 0.23;

    float dist2 = lengthSquared(positionNoise + offsetVec) * 3.0;
    float brightness = (1.0 / dist2 - 0.1) * INTENSITY;

    fragColour = vec4(uCorona.colour.rgb * brightness, brightness * INV_INTENSITY);
}
