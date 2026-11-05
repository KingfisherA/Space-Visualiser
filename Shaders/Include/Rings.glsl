#ifndef SV_RINGS_GLSL
#define SV_RINGS_GLSL

// Planetary ring optics, shared by the ring shader (the rings as seen) and the planet shader (ring
// shadows and ringshine).
//
// Before including, define:
//   RING_MAP                 sampler2D: the ring map, a radial profile of the rings of macroscopic
//                            particles from the inner to the outer radius (only v = 0.5 is read).
//                            A = opacity seen face on, RGB = the particles' albedo (gamma 2.2)
//                            multiplied by the opacity. See src/SV/Universe/Rings.hpp.
//   TRANSMITTANCE_TEXTURE    the planet's precomputed atmospheric transmittance
// and include Atmosphere/Parameters.glsl (for ATMOSPHERE). The faint dusty rings have a separate
// dust map, sampled by the ring shader (see DustOpticalDepth).
//
// Coordinates are "planet space": centred on the planet, y along its rotation axis (the ring plane
// is y = 0), in units of the equatorial radius. The planet is an oblate spheroid; dividing y by the
// polar ratio ("sphere space") turns it into the unit sphere, where the spherical atmosphere and
// shadow tests are done.

vec3 ToSphereSpace(vec3 p, float polarRatio)
{
    return vec3(p.x, p.y / polarRatio, p.z);
}

vec3 FromSphereSpace(vec3 p, float polarRatio)
{
    return vec3(p.x, p.y * polarRatio, p.z);
}

// Henyey-Greenstein phase function, normalised to an average of 1 over the sphere.
// cosTheta is the cosine of the scattering angle (1 = straight on, forwards).
float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5);
}

// Ring particles are of two kinds:
//  - Macroscopic (cm to m) icy bodies, which reflect light back towards the Sun like rough moons.
//  - Micron sized dust, which diffracts light strongly forwards. Dusty, optically thin regions (the
//    D, C and F rings and the Cassini Division) therefore glow when seen against the Sun, while the
//    dense B ring blocks the light and goes dark (as in Cassini's "In Saturn's Shadow" mosaic).
const float RING_PARTICLE_G = -0.35;
const float RING_DUST_G = 0.6;
const float RING_DUST_ALBEDO = 0.8;
// Fraction of the dust's scattering in its narrow diffraction peak (the aureole)
const float RING_DUST_AUREOLE = 0.5;

// Phase function of micron sized dust: a broad forward lobe, plus a narrow diffraction peak that
// makes a bright aureole (a hotspot) around the Sun when it shines through the rings. The peak is
// no narrower than the Sun's disc, which it blurs.
float DustPhase(float cosTheta, float sunAngularRadius)
{
    float gAureole = 1.0 - max(2.0 * sunAngularRadius, 0.05);
    return mix(HenyeyGreenstein(cosTheta, RING_DUST_G), HenyeyGreenstein(cosTheta, gAureole), RING_DUST_AUREOLE);
}

struct RingSample
{
    vec3 albedo;        // Single scattering albedo of the ring particles
    float opticalDepth; // Normal optical depth
    float dustFraction; // Fraction of the optical depth due to dust
};

// radius in equatorial radii. lod is the ring map's mip level (the ring map wraps nothing, so the
// level is chosen by the caller: implicit derivatives are not valid in loops or branches).
RingSample SampleRings(float radius, float innerRadius, float outerRadius, float lod)
{
    float u = (radius - innerRadius) / (outerRadius - innerRadius);
    vec4 texel = textureLod(RING_MAP, vec2(u, 0.5), lod); // Transparent outside the rings

    RingSample ring;
    float opacity = min(texel.a, 0.995);
    ring.opticalDepth = -log(1.0 - opacity);
    ring.albedo = min(pow(texel.rgb / max(texel.a, 0.02), vec3(2.2)), vec3(0.9));
    // Thin rings are dustier than dense ones. Rings of dark particles (Uranus' and Neptune's) are
    // dust poor where they are dense; their dust is in the dust map.
    ring.dustFraction = (0.02 + 0.28 * (1.0 - smoothstep(0.05, 0.6, ring.opticalDepth))) *
                        smoothstep(0.05, 0.3, max(ring.albedo.r, max(ring.albedo.g, ring.albedo.b)));
    return ring;
}

// Bidirectional reflectance of a ring layer as I/F, from single scattering in a thin plane
// parallel slab (Chandrasekhar 1960; as used for Saturn's rings by Cuzzi et al.).
//   toSun, toViewer: unit vectors in planet space (the ring normal is +-y)
// Light reaching the viewer on the lit face is diffusely reflected; on the unlit face it is
// diffusely transmitted through the layer.
vec3 RingReflectance(RingSample ring, vec3 toSun, vec3 toViewer, float sunAngularRadius)
{
    float mu0 = max(abs(toSun.y), 1e-3);    // Incidence
    float mu = max(abs(toViewer.y), 1e-3);  // Emission
    float tau = ring.opticalDepth;

    // Scattering angle: 0 when looking towards the Sun through the rings (forward scattering)
    float cosTheta = -dot(toSun, toViewer);
    vec3 albedoPhase = (1.0 - ring.dustFraction) * ring.albedo * HenyeyGreenstein(cosTheta, RING_PARTICLE_G) +
                       ring.dustFraction * RING_DUST_ALBEDO * DustPhase(cosTheta, sunAngularRadius);

    float slab;
    if (toSun.y * toViewer.y > 0.0)
    {
        // Lit face
        slab = mu0 / (mu + mu0) * (1.0 - exp(-tau * (1.0 / mu + 1.0 / mu0)));
    }
    else if (abs(mu - mu0) > 1e-3)
    {
        // Unlit face
        slab = mu0 / (mu - mu0) * (exp(-tau / mu) - exp(-tau / mu0));
    }
    else
    {
        slab = tau / mu * exp(-tau / mu);
    }
    return albedoPhase * 0.25 * slab;
}

// Fraction of light from the planet's Sun-facing side that passes the ring layer, seen at emission
// cosine mu
float RingTransmittance(RingSample ring, float mu)
{
    return exp(-ring.opticalDepth / max(mu, 1e-3));
}

// Transmittance of sunlight past the planet, towards the point p (planet space). Accounts for the
// Sun's angular radius (a soft penumbra) and the planet's atmosphere (which reddens and dims the
// light grazing the limb).
vec3 PlanetShadow(vec3 p, vec3 toSun, float polarRatio, float sunAngularRadius)
{
    vec3 origin = ToSphereSpace(p, polarRatio);
    vec3 direction = normalize(ToSphereSpace(toSun, polarRatio));
    float tClosest = -dot(origin, direction);
    if (tClosest <= 0.0)
        return vec3(1.0); // Moving away from the planet

    // Closest approach of the ray to the planet's centre, in planet radii
    float closest = length(origin + direction * tClosest);
    float penumbra = max(tClosest * tan(sunAngularRadius), 1e-3);
    float visible = smoothstep(1.0 - penumbra, 1.0 + penumbra, closest);

    AtmosphereParameters atmosphere = ATMOSPHERE;
    float topRatio = atmosphere.top_radius / atmosphere.bottom_radius;
    if (closest >= topRatio)
        return vec3(visible);

    // The ray crosses the atmosphere twice (horizontally at its lowest point)
    float r = clamp(closest * atmosphere.bottom_radius, atmosphere.bottom_radius, atmosphere.top_radius);
    vec3 halfTransmittance = GetTransmittanceToTopAtmosphereBoundary(atmosphere, TRANSMITTANCE_TEXTURE, r, 0.0);
    return visible * halfTransmittance * halfTransmittance;
}

// Transmittance of sunlight through the rings onto the point p (planet space)
vec3 RingShadow(vec3 p, vec3 toSun, float innerRadius, float outerRadius, float sunAngularRadius)
{
    float t = -p.y / toSun.y;
    if (t <= 0.0 || abs(toSun.y) < 1e-4)
        return vec3(1.0); // The rings are not between the point and the Sun

    vec3 hit = p + toSun * t;
    float hitRadius = length(hit.xz);
    // Blur by the Sun's size seen through the rings (the shadow's penumbra), and by the pixel's
    // footprint so the fine structure doesn't alias. Called in uniform control flow, for fwidth.
    float blur = max(t * tan(sunAngularRadius), fwidth(hitRadius)) / (outerRadius - innerRadius);
    float lod = log2(max(blur * float(textureSize(RING_MAP, 0).x), 1.0));
    RingSample ring = SampleRings(hitRadius, innerRadius, outerRadius, lod);
    return vec3(RingTransmittance(ring, abs(toSun.y)));
}

// Irradiance on the point p (planet space, surface normal `normal`) from sunlight scattered by the
// rings ("ringshine"), relative to the solar irradiance. This lights the night side.
vec3 RingshineIrradiance(vec3 p, vec3 normal, vec3 toSun, float innerRadius, float outerRadius,
                         float polarRatio, float sunAngularRadius)
{
    const int RADIAL_SAMPLES = 4;
    const int AZIMUTH_SAMPLES = 12;
    // A coarse mip: each sample stands for a wide band of the rings
    float lod = log2(float(textureSize(RING_MAP, 0).x) / float(RADIAL_SAMPLES));

    float width = outerRadius - innerRadius;
    vec3 irradiance = vec3(0.0);
    for (int i = 0; i < RADIAL_SAMPLES; i++)
    {
        float radius = innerRadius + (float(i) + 0.5) / float(RADIAL_SAMPLES) * width;
        RingSample ring = SampleRings(radius, innerRadius, outerRadius, lod);
        float area = 2.0 * PI * radius / float(AZIMUTH_SAMPLES) * width / float(RADIAL_SAMPLES);
        for (int j = 0; j < AZIMUTH_SAMPLES; j++)
        {
            float angle = (float(j) + 0.5) / float(AZIMUTH_SAMPLES) * 2.0 * PI;
            vec3 ringPoint = vec3(cos(angle), 0.0, sin(angle)) * radius;
            vec3 toRing = ringPoint - p;
            float distance2 = dot(toRing, toRing);
            toRing *= inversesqrt(distance2);

            // Below the horizon (this also hides the rings behind the planet)
            float cosSurface = dot(normal, toRing);
            if (cosSurface <= 0.0)
                continue;

            vec3 radiance = RingReflectance(ring, toSun, -toRing, sunAngularRadius) *
                            PlanetShadow(ringPoint, toSun, polarRatio, sunAngularRadius);
            irradiance += radiance * cosSurface * abs(toRing.y) * area / distance2;
        }
    }
    // I/F is radiance relative to solar irradiance / pi
    return irradiance / PI;
}

// The dust map's optical depths are in this unit (mirrors SV::Rings::DUST_DEPTH_UNIT)
const float DUST_DEPTH_UNIT = 1e-5;
// The gain exaggerates the faint dusty rings only up to about this optical depth
const float DUST_GAIN_LIMIT = 0.02;

// Normal optical depth per colour channel of the dust (a texel of the dust map), exaggerated by
// `gain` while faint (the optical depths of the dusty rings are about 1e-5, hundreds of times
// fainter than the main rings even against the Sun). Rings already denser than the limit keep their
// true optical depth.
vec3 DustOpticalDepth(vec3 texel, float gain)
{
    vec3 opticalDepth = texel * DUST_DEPTH_UNIT;
    vec3 exaggerated = opticalDepth * gain / (1.0 + opticalDepth * gain / DUST_GAIN_LIMIT);
    return max(opticalDepth, exaggerated);
}

// I/F of an optically thin layer of dust (either face), for a normal optical depth per channel
vec3 DustReflectance(vec3 opticalDepth, vec3 toSun, vec3 toViewer, float sunAngularRadius)
{
    // The dusty rings are hundreds to thousands of km thick (Saturn's E ring and Jupiter's halo
    // more than that), which limits the path through them when seen edge on
    float mu = max(abs(toViewer.y), 0.05);
    return RING_DUST_ALBEDO * DustPhase(-dot(toSun, toViewer), sunAngularRadius) * opticalDepth / (4.0 * mu);
}

#endif // SV_RINGS_GLSL
