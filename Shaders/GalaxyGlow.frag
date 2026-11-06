#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Galaxy.glsl"

layout(location = 0) in vec2 vNdc;

// Radiance (HDR)
layout(location = 0) out vec4 fragColour;

// The glow of the galaxy's unresolved stars and gas along the pixel's ray, dimmed by the dust in
// front of it. Steps are sized to the structure: short near the plane (where the disc, arms and dust
// are), longer away from it, and short near the camera.
const int MAX_STEPS = 256;
// A step changes the height by at most this fraction of the height (plus the young disc's scale height)
const float VERTICAL_STEP = 0.25;
// A step is at most this fraction of the distance from the camera, plus NEAR_STEP
const float DISTANCE_STEP = 0.1;
const float NEAR_STEP = 20.0;
const float MIN_STEP = 2.0;
// Steps are at most this fraction of the disc's radius
const float MAX_STEP_FRACTION = 1.0 / 60.0;
// The glow is drawn at half resolution: the companions are widened to its pixels
const float GLOW_PIXEL = 2.0;

// Interleaved gradient noise (Jimenez 2014): offsets each pixel's samples, trading banding for fine noise
float Dither(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

void main()
{
    // The ray through this pixel, in galactic coordinates
    vec3 directionView = normalize(vec3(vNdc.x / uFrame.projection[0][0], vNdc.y / uFrame.projection[1][1], -1.0));
    vec3 direction = transpose(mat3(uFrame.view)) * directionView;
    vec3 from = uFrame.cameraPosition.xyz - uGalaxy.centre.xyz;

    vec3 companionsInFront, companionsBehind;
    CompanionLight(from, direction, GLOW_PIXEL * uGalaxy.view.x, companionsInFront, companionsBehind);

    float tNear = 0.0, tFar = 1e30;
    if (!ClipToGalaxy(from, direction, GalaxyHalfThickness(), tNear, tFar))
    {
        fragColour = vec4(companionsInFront + companionsBehind, 1.0);
        return;
    }

    float dither = Dither(gl_FragCoord.xy);
    vec3 light = vec3(0.0);
    vec3 transmittance = vec3(1.0);
    float t = tNear;
    for (int i = 0; i < MAX_STEPS; i++)
    {
        if (t >= tFar)
            break;
        float y = from.y + direction.y * t;
        float dt = VERTICAL_STEP * (abs(y) + uGalaxy.disc.z) / max(abs(direction.y), 1e-4);
        dt = clamp(min(dt, DISTANCE_STEP * t + NEAR_STEP), MIN_STEP, MAX_STEP_FRACTION * uGalaxy.centre.w);
        dt = min(dt, tFar - t);

        vec3 p = from + direction * (t + dither * dt);
        GalaxyPoint g = LocateInGalaxy(p);
        vec3 emission = GlowEmissionAt(p, g);
        float dust = abs(p.y) < DustHalfThickness() ? DustDensityAt(p, g) : 0.0;
        vec3 depth = uGalaxy.dust.x * dust * dt * DUST_REDDENING;
        vec3 stepTransmittance = exp(-depth);
        // The light given off along the step, dimmed by the dust in front of it within the step:
        // (1 - e^-depth) / depth, which tends to 1 as the depth does
        vec3 selfShadow = mix(vec3(1.0), (1.0 - stepTransmittance) / max(depth, vec3(1e-6)), step(vec3(1e-4), depth));
        light += transmittance * emission * dt * selfShadow;
        transmittance *= stepTransmittance;
        if (max(transmittance.r, max(transmittance.g, transmittance.b)) < 1e-3)
            break;
        t += dt;
    }
    fragColour = vec4(light + companionsInFront + companionsBehind * transmittance, 1.0);
}
