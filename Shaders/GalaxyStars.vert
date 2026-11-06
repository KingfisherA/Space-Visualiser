#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Galaxy.glsl"

layout(location = 0) in vec2 aCorner;  // The quad's corner, in [-1, 1]
layout(location = 1) in vec4 aStar;    // xyz = position (galactic coordinates), w = luminosity
layout(location = 2) in vec4 aColour;  // rgb = colour (linear, luminance 1), w = radius (0 for stars)

layout(location = 0) out vec2 vCorner;
layout(location = 1) flat out vec3 vRadiance;

// Each star is a point: a Gaussian this many pixels wide (one standard deviation), drawn out to
// EXTENT_SIGMAS. Its peak is set so the light it adds up to is its flux, luminosity / distance^2,
// so stars keep the same brightness however finely they are drawn. Nebulae (H II regions) are drawn
// the same way, but at least their radius wide.
const float STAR_SIGMA_PIXELS = 0.7;
const float EXTENT_SIGMAS = 3.0;
// Closer stars are drawn as if this far away, and no star is brighter than MAX_RADIANCE
const float MIN_DISTANCE = 1.0;
const float MAX_RADIANCE = 2000.0;
// Samples of the dust between a star and the camera
const int EXTINCTION_STEPS = 16;

// The fraction of a star's light (by channel) that gets through the dust to the camera
vec3 Extinction(vec3 star, vec3 camera)
{
    vec3 segment = camera - star;
    float len = length(segment);
    if (len < 1e-3)
        return vec3(1.0);
    vec3 direction = segment / len;
    float tNear = 0.0, tFar = len;
    if (!ClipToGalaxy(star, direction, DustHalfThickness(), tNear, tFar))
        return vec3(1.0);

    float dt = (tFar - tNear) / float(EXTINCTION_STEPS);
    float density = 0.0;
    for (int i = 0; i < EXTINCTION_STEPS; i++)
        density += DustDensity(star + direction * (tNear + (float(i) + 0.5) * dt));
    return exp(-uGalaxy.dust.x * density * dt * DUST_REDDENING);
}

void main()
{
    vec3 position = aStar.xyz + uGalaxy.centre.xyz;
    vec3 camera = uFrame.cameraPosition.xyz;
    float distance = max(length(camera - position), MIN_DISTANCE);

    float sigma = max(STAR_SIGMA_PIXELS * uGalaxy.view.x, aColour.w / distance); // Radians
    float brightness = aColour.w > 0.0 ? uGalaxy.view.z : uGalaxy.view.y;
    float peak = aStar.w * brightness / (distance * distance * 6.28318531 * sigma * sigma);
    vRadiance = min(aColour.rgb * peak, vec3(MAX_RADIANCE)) * Extinction(aStar.xyz, camera - uGalaxy.centre.xyz);
    vCorner = aCorner;

    // Facing the camera, spanning its right and up axes
    mat4 view = uFrame.view;
    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);
    float halfSize = EXTENT_SIGMAS * sigma * distance;
    gl_Position = uFrame.viewProjection * vec4(position + (cameraRight * aCorner.x + cameraUp * aCorner.y) * halfSize, 1.0);
}
