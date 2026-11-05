#ifndef SV_PLANET_PUSH_CONSTANTS_GLSL
#define SV_PLANET_PUSH_CONSTANTS_GLSL

// Mirrors SV::PlanetPushConstants (128 bytes)
layout(push_constant) uniform PlanetPushConstants
{
    mat4 transform; // Sky mesh (top of the atmosphere) to world. The translation is the planet's centre.
    // Columns of the planet's rotation (planet space to world). w components:
    //   [0].w = lit surfaces: the diffuse map's albedo scale; otherwise the external atmosphere
    //           intensity. [1].w = flags (PLANET_*),
    //   [2].w = world units per equatorial radius
    vec4 axes[3];
    // x = polar radius / equatorial radius, y = ring inner radius, z = ring outer radius (both in
    // equatorial radii), w = rotation speed (texture turns per unit of planet time)
    vec4 shape;
} uPlanet;

const int PLANET_HAS_SPECULAR = 1;
const int PLANET_HAS_CLOUDS = 2;
const int PLANET_HAS_RINGS = 4;
const int PLANET_HAS_NIGHT = 8;
// Light the surface with the atmosphere's transmitted sunlight (gas giants) rather than Earth's
// stylised shading
const int PLANET_LIT_SURFACE = 16;

mat3 GetPlanetRotation()
{
    return mat3(uPlanet.axes[0].xyz, uPlanet.axes[1].xyz, uPlanet.axes[2].xyz);
}

#endif // SV_PLANET_PUSH_CONSTANTS_GLSL
