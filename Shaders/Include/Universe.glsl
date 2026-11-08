#ifndef SV_UNIVERSE_GLSL
#define SV_UNIVERSE_GLSL

// The universe view's shared values (see SV::UniverseRenderer in src/SV/Universe/UniverseRenderer.hpp).
// Positions are in parsecs from the Sun along the world's axes, split into a float and the
// remainder (high + low), so they can be taken from the camera's without losing precision far
// from the Sun. Light is in units of a magnitude 0 star's flux.

// Mirrors UniverseRenderer::MAX_STAR_CLOSE_UPS in src/SV/Universe/UniverseRenderer.hpp
#define MAX_STAR_CLOSE_UPS 4

// Mirrors UniverseUniforms in src/SV/Universe/UniverseRenderer.cpp (std140)
layout(std140, set = 0, binding = 0) uniform UniverseUniforms
{
    mat4 viewProjection; // From positions relative to the camera (pc)
    mat4 view;           // The camera's orientation
    vec4 cameraHigh;     // xyz = the camera's position (high part)
    vec4 cameraLow;      // xyz = the low part
    vec4 viewport;       // x = pixel angle (radians), y = width, z = height (pixels), w = time (s)
    vec4 stars;          // x = star brightness, y = gain for stars, z = zoom (pc), w = the Sun's visibility
    vec4 extended;       // x = galaxy surface gain, y = gain for unresolved galaxies, z = nebula surface gain, w = nebula markers
    vec4 markers;        // x = neutron star markers, y = black hole markers, z = stars shown, w = exposure
    // The galaxy, nebula and point shown close up (xyz = their positions' high parts), and how
    // much of each of their sprites or markers is still drawn (w)
    vec4 hiddenGalaxy;
    vec4 hiddenNebula;
    vec4 hiddenPoint;
    vec4 hiddenMilkyWay; // The Milky Way's sprite, while the galaxy model draws it
    // The nebula close up's region, whose other sprites are hidden as it fades in: x = its radius
    // across the line of sight from the Sun, y = its depth along it (pc)
    vec4 hiddenNebulaRegion;
    // The stars shown close up (xyz = their positions' high parts), and how much of each of their
    // points is still drawn (w)
    vec4 hiddenStars[MAX_STAR_CLOSE_UPS];
} uUniverse;

// Four channels of tileable fBm, each spread evenly over [0, 1] (shared with the galaxy and nebulae)
layout(set = 0, binding = 1) uniform sampler3D uNoise;

// Mirrors UniversePushConstants in src/SV/Universe/UniverseRenderer.cpp
layout(push_constant) uniform UniversePushConstants
{
    vec4 pass; // x = which pass (PASS_*)
} uPass;

const int PASS_POINTS = 0;         // Stars, the Sun, neutron stars and black holes
const int PASS_GALAXY_POINTS = 1;  // Galaxies too small to see as sprites
const int PASS_GALAXY_DISCS = 2;   // Spiral and irregular galaxies' discs
const int PASS_GALAXY_SPHEROIDS = 3; // Elliptical galaxies, and spirals' bulges
const int PASS_NEBULA_POINTS = 4;
const int PASS_NEBULA_SPRITES = 5;

// Mirrors SV::SpriteShape in src/SV/Universe/Catalogue.hpp
const int SHAPE_SPIRAL = 0;
const int SHAPE_BARRED_SPIRAL = 1;
const int SHAPE_ELLIPTICAL = 2;
const int SHAPE_IRREGULAR = 3;
const int SHAPE_PLANETARY_NEBULA = 4;
const int SHAPE_HII_REGION = 5;
const int SHAPE_SHELL_REMNANT = 6;
const int SHAPE_FILLED_REMNANT = 7;

// The flux (solar luminosities per square parsec) of a magnitude 0 star in V
const float REFERENCE_FLUX = 0.855;
const float PI = 3.14159265;

// A position relative to the camera (pc)
vec3 FromCamera(vec3 high, vec3 low)
{
    return (high - uUniverse.cameraHigh.xyz) + (low - uUniverse.cameraLow.xyz);
}

// A clip position at a point relative to the camera, moved by `pixels` across the screen. Depth
// isn't tested, so z is kept halfway (inside every API's clip volume).
vec4 ClipAt(vec3 relative, vec2 pixels)
{
    vec4 clip = uUniverse.viewProjection * vec4(relative, 1.0);
    clip.xy += pixels * 2.0 / uUniverse.viewport.yz * clip.w;
    clip.z = 0.5 * clip.w;
    return clip;
}

// How much of an object (by its position's high part) is drawn: less of one shown close up.
// (Matched within a hair of its distance, as the shader's arithmetic needn't be exact.)
float Shown(vec3 high, vec4 hidden)
{
    return distance(high, hidden.xyz) <= 1e-5 * length(hidden.xyz) + 1e-6 ? hidden.w : 1.0;
}

// How much of a nebula is drawn: none of those within the region of the nebula shown close up (the
// parts of it the close-up draws, such as M43 by the Orion Nebula), as it fades in.
// Mirrors UniverseRenderer::IsInNebulaRegion.
float ShownNebula(vec3 high)
{
    vec4 hidden = uUniverse.hiddenNebula;
    if (uUniverse.hiddenNebulaRegion.x <= 0.0)
        return 1.0;
    vec3 direction = normalize(hidden.xyz);
    vec3 relative = high - hidden.xyz;
    float along = dot(relative, direction);
    bool inside = length(relative - along * direction) <= uUniverse.hiddenNebulaRegion.x && abs(along) <= uUniverse.hiddenNebulaRegion.y;
    return inside ? hidden.w : 1.0;
}

// Off screen, for points not drawn
const vec4 CULLED = vec4(2.0, 2.0, 2.0, 1.0);

// A point of `light` (in reference fluxes) spread over a Gaussian a pixel or so across: its size
// grows with its brightness (as in photographs), and its peak is set so the light it adds up to
// is `light` however wide it is. Returns the peak and the quad's half size (pixels).
const float POINT_SIGMA = 0.7;     // Pixels, for faint points
const float POINT_EXTENT = 3.0;    // Standard deviations drawn
const float MAX_POINT_SIGMA = 6.0;
vec2 PointShape(float light)
{
    float apparent = light * uUniverse.markers.w;
    float sigma = min(POINT_SIGMA * (1.0 + 0.8 * log(1.0 + apparent / 2.0)), MAX_POINT_SIGMA);
    return vec2(light / (2.0 * PI * sigma * sigma), POINT_EXTENT * sigma);
}

#endif // SV_UNIVERSE_GLSL
