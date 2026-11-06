#ifndef SV_FRAME_GLSL
#define SV_FRAME_GLSL

// Bodies that can cast shadows (eclipses), at most this many
const int MAX_OCCLUDERS = 16;

// Per-frame scene values shared by every scene shader (set 0). Mirrors SV::FrameUniforms (std140).
layout(std140, set = 0, binding = 0) uniform FrameUniforms
{
    mat4 viewProjection;
    mat4 view;
    mat4 projection;
    vec4 cameraPosition; // xyz = camera position in world space
    vec4 cameraCentre;   // xyz = the point the camera orbits
    vec4 sun;            // xyz = the Sun's position in world space, w = the Sun's radius
    vec4 times;          // x = planet animation time, y = star animation time
    vec4 display;        // x = exposure
    vec4 occluderCount;  // x = the number of occluders
    // xyz = centre in world space, w = radius (negative for a body whose atmosphere bends reddened
    // sunlight into its shadow, like Earth's in a lunar eclipse)
    vec4 occluders[MAX_OCCLUDERS];
} uFrame;

// Maps scene radiance to display colour
vec3 ToneMap(vec3 radiance)
{
    return pow(vec3(1.0) - exp(-radiance * uFrame.display.x), vec3(1.0 / 2.2));
}

// Sunlight reaching `point` past the bodies between it and the Sun: 1 in full sunlight, falling
// through the penumbra to 0 in an umbra (or a dim red in Earth's). `self` is the centre of the body
// the point is on, which doesn't shadow itself this way.
vec3 SunVisibility(vec3 point, vec3 self)
{
    vec3 toSun = uFrame.sun.xyz - point;
    float sunDistance = length(toSun);
    vec3 sunDirection = toSun / sunDistance;
    float sunRadius = uFrame.sun.w / sunDistance; // Angular radius (small angles)

    vec3 light = vec3(1.0);
    int count = int(uFrame.occluderCount.x + 0.5);
    for (int i = 0; i < MAX_OCCLUDERS; i++)
    {
        if (i >= count)
            break;
        vec4 occluder = uFrame.occluders[i];
        float radius = abs(occluder.w);
        if (distance(occluder.xyz, self) < 1e-3 * radius)
            continue;
        vec3 toOccluder = occluder.xyz - point;
        float along = dot(toOccluder, sunDirection);
        if (along <= 0.0 || along >= sunDistance)
            continue;
        float occluderDistance = length(toOccluder);
        if (occluderDistance <= radius)
            continue;

        // The discs' angular radii and separation, in the Sun's angular radii. The separation is
        // taken from the cross product, which (unlike acos) is precise for the tiny angles involved.
        float occluderRadius = asin(radius / occluderDistance) / sunRadius;
        float separation = atan(length(cross(toOccluder, sunDirection)), along) / sunRadius;
        if (separation >= occluderRadius + 1.0)
            continue;

        // Fraction of the Sun's disc covered: all of it in an umbra, (r / R)^2 in a transit or an
        // annular eclipse, eased across the penumbra
        float covered;
        if (occluderRadius >= 1.0)
            covered = 1.0 - smoothstep(occluderRadius - 1.0, occluderRadius + 1.0, separation);
        else
            covered = occluderRadius * occluderRadius * (1.0 - smoothstep(1.0 - occluderRadius, 1.0 + occluderRadius, separation));

        // Earth's atmosphere refracts sunlight into its umbra, reddened by its long path through the air
        vec3 umbra = occluder.w < 0.0 ? vec3(0.012, 0.0035, 0.001) : vec3(0.0);
        light *= mix(vec3(1.0), umbra, covered);
    }
    return light;
}

#endif // SV_FRAME_GLSL
