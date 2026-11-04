#ifndef SV_PLANET_PUSH_CONSTANTS_GLSL
#define SV_PLANET_PUSH_CONSTANTS_GLSL

// Mirrors SV::PlanetPushConstants (112 bytes)
layout(push_constant) uniform PlanetPushConstants
{
    mat4 transform;    // Sky sphere transform; translation column is the planet's centre
    // Columns of the planet's normal matrix. w components:
    //   [0].w = external atmosphere intensity, [1].w = map flags (PLANET_HAS_*)
    vec4 normalMatrix[3];
} uPlanet;

const int PLANET_HAS_SPECULAR = 1;
const int PLANET_HAS_CLOUDS = 2;
const int PLANET_HAS_RING_SHADOW = 4;
const int PLANET_HAS_NIGHT = 8;

mat3 GetPlanetNormalMatrix()
{
    return mat3(uPlanet.normalMatrix[0].xyz, uPlanet.normalMatrix[1].xyz, uPlanet.normalMatrix[2].xyz);
}

#endif // SV_PLANET_PUSH_CONSTANTS_GLSL
