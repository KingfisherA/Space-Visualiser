#ifndef SV_GALAXY_GLSL
#define SV_GALAXY_GLSL

// The density model of the galaxy (see SV::GalaxyParameters in src/SV/Universe/Galaxy.hpp). Galactic
// coordinates are in parsecs, with the centre at the origin and the disc in the XZ plane. The arm
// geometry mirrors ArmGeometry in src/SV/Universe/Galaxy.cpp, which places the stars.

// Mirrors MAX_COMPANIONS in src/SV/Universe/Galaxy.hpp
#define MAX_COMPANIONS 3

// Mirrors GalaxyUniforms in src/SV/Universe/Galaxy.cpp (std140)
layout(std140, set = 1, binding = 0) uniform GalaxyUniforms
{
    vec4 centre;  // xyz = the galactic centre in render coordinates, w = the disc's radius
    vec4 bar;     // x = angle (radians), y = half-length, z = width ratio, w = height ratio
    vec4 arms;    // x = count, y = tan(pitch), z = radius the major arms leave the bar at, w = width
    vec4 disc;    // x = scale length, y = scale height, z = young scale height, w = dust scale height
    vec4 glow;    // x = bulge, y = disc, z = arms, w = nebulae (face-on brightness at their brightest)
    vec4 dust;    // x = face-on optical depth through an arm, y = lane offset, z = bulge radius
    vec4 bulgeColour;  // rgb, luminance 1
    vec4 discColour;
    vec4 youngColour;
    vec4 nebulaColour;
    vec4 view;    // x = pixel angle (radians), y = star brightness, z = nebula (H II region) brightness
    vec4 shape;   // x = minor arms' strength, y = the bar's share of the central light, z = companion count
    vec4 ring;    // x = radius, y = width, z = strength
    // Each companion: xyz = centre (galactic coordinates), w = its halo's standard deviation; xyz =
    // its stretch, w = its luminosity; rgb = colour, a = how bright its core and nucleus are
    vec4 companionCentre[MAX_COMPANIONS];
    vec4 companionStretch[MAX_COMPANIONS];
    vec4 companionColour[MAX_COMPANIONS];
} uGalaxy;

const float TAU = 6.28318531;

// Tileable noise: four independent channels of fBm, each spread evenly over [0, 1]. It has no mip
// levels, so it is sampled with textureLod (also in vertex shaders and branches).
layout(set = 1, binding = 1) uniform sampler3D uGalaxyNoise;

// Clumps: the noise repeating every CLUMP_NOISE_PERIOD parsecs, turned off the axes so the repeats
// don't line up with them
const float CLUMP_NOISE_PERIOD = 9000.0;
const mat3 CLUMP_NOISE_ROTATION = mat3(vec3(0.648638, 0.682114, -0.337622),
                                       vec3(-0.574003, 0.729721, 0.371520),
                                       vec3(0.499789, -0.047186, 0.864861));
// Filaments: the noise in spiral coordinates, so it is drawn out along the arms. Across them, it
// repeats SPIRAL_NOISE_TURNS times a turn (a whole number, so it meets itself around the galaxy);
// along them, every 1 / SPIRAL_NOISE_ALONG in ln(radius); vertically, every SPIRAL_NOISE_HEIGHT.
const float SPIRAL_NOISE_TURNS = 5.0;
const float SPIRAL_NOISE_ALONG = 4.0;
const float SPIRAL_NOISE_HEIGHT = 1500.0;
// Inside the arms, where the spirals would wind ever tighter, the filaments are clouds instead,
// repeating every INNER_NOISE_PERIOD
const float INNER_NOISE_PERIOD = 4000.0;

// Dust absorbs blue light more than red (relative optical depth by channel)
const vec3 DUST_REDDENING = vec3(0.65, 1.0, 1.45);

// How far from the plane there is anything to see: the bar's thickness, and the dust's
float GalaxyHalfThickness()
{
    return max(max(2.5 * uGalaxy.bar.y * uGalaxy.bar.w, 8.0 * uGalaxy.disc.y), 10.0 * uGalaxy.dust.z);
}

float DustHalfThickness()
{
    return 6.0 * uGalaxy.disc.w;
}

float Square(float x)
{
    return x * x;
}

// Vertical profile, normalised so it integrates to 1 through the plane
float Vertical(float y, float scaleHeight)
{
    return exp(-abs(y) / scaleHeight) / (2.0 * scaleHeight);
}

// Where the galaxy's structure is, at a point in the plane
struct GalaxyPoint
{
    float r;         // Distance from the centre
    // Signed distances across the spiral arms either side: the one inside the point (x, positive)
    // and the one outside it (y, negative, as the point is on its inner, concave edge)
    vec2 offsets;
    vec2 majors;     // 1 for the major arms (the even ones), 0 for the minor ones
    float envelope;  // The arms' strength along their length
    vec4 filaments;  // Noise drawn out along the arms (r: dust, g: young stars, b: nebulae)
    float clumps;    // Noise in clumps
};

// The arms: fading in from the bar's ends, and out at the disc's edge. Mirrors ArmGeometry::Envelope.
float ArmEnvelope(float r)
{
    return smoothstep(0.6 * uGalaxy.arms.z, uGalaxy.arms.z, r) * smoothstep(uGalaxy.centre.w, 0.8 * uGalaxy.centre.w, r) *
           exp(-(r - uGalaxy.arms.z) / (1.6 * uGalaxy.disc.x));
}

// Young stars: mostly past the bar's ends, where the arms are bluest (mirrors ArmGeometry::YoungEnvelope)
float YoungEnvelope(float r, float envelope)
{
    return envelope * smoothstep(0.7 * uGalaxy.arms.z, 1.6 * uGalaxy.arms.z, r);
}

GalaxyPoint LocateInGalaxy(vec3 p)
{
    GalaxyPoint g;
    g.r = length(p.xz);
    float tanPitch = uGalaxy.arms.y;
    float spacing = TAU / uGalaxy.arms.x;
    // psi is constant along an arm: the arms trail, so their angle falls as the radius grows
    float lnr = log(max(g.r, 1.0) / uGalaxy.arms.z);
    float psi = atan(p.z, p.x) + lnr / tanPitch;
    float x = (psi - uGalaxy.bar.x) / spacing;
    float inside = floor(x);
    g.majors = vec2(mod(inside, 2.0) < 0.5 ? 1.0 : 0.0, mod(inside + 1.0, 2.0) < 0.5 ? 1.0 : 0.0);
    // Across an arm: the arc between it and the point, times the sine of the pitch
    float sinPitch = tanPitch * inversesqrt(1.0 + tanPitch * tanPitch);
    g.offsets = vec2(x - inside, x - inside - 1.0) * (spacing * g.r * sinPitch);
    g.envelope = ArmEnvelope(g.r);

    g.clumps = textureLod(uGalaxyNoise, CLUMP_NOISE_ROTATION * p / CLUMP_NOISE_PERIOD, 0.0).a;
    // Bent a little by the clumps, so they aren't all parallel
    vec3 spiral = vec3(psi * (SPIRAL_NOISE_TURNS / TAU), lnr * SPIRAL_NOISE_ALONG, p.y / SPIRAL_NOISE_HEIGHT);
    spiral.x += 0.35 * (g.clumps - 0.5);
    g.filaments = textureLod(uGalaxyNoise, spiral, 0.0);
    float spiralWeight = smoothstep(0.4 * uGalaxy.arms.z, 0.8 * uGalaxy.arms.z, g.r);
    if (spiralWeight < 1.0)
    {
        vec4 clouds = textureLod(uGalaxyNoise, CLUMP_NOISE_ROTATION * (p.zyx + vec3(1700.0, 0.0, 900.0)) / INNER_NOISE_PERIOD, 0.0);
        g.filaments = mix(clouds, g.filaments, spiralWeight);
    }
    return g;
}

// The arms either side of a point: a Gaussian across each, `width` wide (one standard deviation),
// moved `shift` towards their inner edges, with the minor arms weighted by `minorWeight`. Adding
// both (rather than taking the nearest) keeps it smooth between them. Mirrors ArmGeometry::Arms.
float Arms(GalaxyPoint g, float shift, float width, float minorWeight)
{
    vec2 across = (g.offsets + shift) / width;
    return dot(exp(-0.5 * across * across), mix(vec2(minorWeight), vec2(1.0), g.majors));
}

// The ring (if any): its strength at radius r
float Ring(float r)
{
    return uGalaxy.ring.z * exp(-0.5 * Square((r - uGalaxy.ring.x) / max(uGalaxy.ring.y, 1.0)));
}

// Dust density at `p` (galactic coordinates, located at `g`): optical depth per parsec divided by
// the face-on depth through an arm
float DustDensityAt(vec3 p, GalaxyPoint g)
{
    float vertical = Vertical(p.y, uGalaxy.disc.w);

    // Lanes along the inner edges of the arms, reaching in around the bar's ends
    float r0 = uGalaxy.arms.z;
    float laneEnvelope = smoothstep(0.35 * r0, 0.7 * r0, g.r) * smoothstep(uGalaxy.centre.w, 0.8 * uGalaxy.centre.w, g.r) *
                         exp(-(g.r - r0) / (1.6 * uGalaxy.disc.x));
    float lane = Arms(g, uGalaxy.dust.y, 0.45 * uGalaxy.arms.w, 1.0) * laneEnvelope;
    // Thinner dust through the disc, reaching further out than its stars (so edge-on, the disc's
    // rim is dark against the bulge), thickest around the bar
    float disc = 0.6 * exp(-(g.r - r0) / (2.0 * uGalaxy.disc.x)) *
                 smoothstep(0.35 * r0, 0.7 * r0, g.r) * smoothstep(uGalaxy.centre.w, 0.9 * uGalaxy.centre.w, g.r);
    // A dense ring around the centre
    float bulgeRadius = uGalaxy.dust.z;
    float centralRing = 0.8 * exp(-0.5 * Square((g.r - 0.6 * bulgeRadius) / (0.3 * bulgeRadius)));

    // Broken into filaments along the arms, and clouds
    float filaments = smoothstep(0.35, 0.9, g.filaments.r);
    float clouds = 0.35 + 1.3 * smoothstep(0.25, 0.75, g.clumps);
    return (((lane + Ring(g.r)) * (0.35 + filaments) + disc * 2.2 * filaments) * clouds + centralRing) * vertical;
}

float DustDensity(vec3 p)
{
    if (abs(p.y) > DustHalfThickness())
        return 0.0;
    return DustDensityAt(p, LocateInGalaxy(p));
}

// Light given off per parsec at `p` (galactic coordinates, located at `g`), from the unresolved
// stars and gas
vec3 GlowEmissionAt(vec3 p, GalaxyPoint g)
{
    float r0 = uGalaxy.arms.z;

    // The bar, in its own frame, with a boxy (rather than elliptical) outline
    float c = cos(uGalaxy.bar.x), s = sin(uGalaxy.bar.x);
    vec2 inBar = vec2(c * p.x + s * p.z, -s * p.x + c * p.z);
    float a = uGalaxy.bar.y;
    vec3 q = abs(vec3(inBar.x / a, p.y / (a * uGalaxy.bar.w), inBar.y / (a * uGalaxy.bar.z)));
    float m = pow(pow(q.x, 3.0) + pow(q.y, 3.0) + pow(q.z, 3.0), 1.0 / 3.0);
    // Flat topped, so the bar is bright along its length rather than only at its centre: it falls
    // to 1/e at BAR_SCALE of its half-length (mirrors src/SV/Universe/Galaxy.cpp, which places the
    // bar's stars). Normalised, like the bulge, so it integrates to 1 face-on through the centre.
    const float BAR_SCALE = 0.45;
    float bar = exp(-Square(m / BAR_SCALE)) / (1.77245385 * BAR_SCALE * a * uGalaxy.bar.w);
    float bulgeRadius = uGalaxy.dust.z;
    float bulge = exp(-length(p) / bulgeRadius) / (2.0 * bulgeRadius);
    vec3 light = uGalaxy.bulgeColour.rgb * uGalaxy.glow.x * mix(bulge, bar, uGalaxy.shape.y);

    // The arms' cores, and the young stars spread further either side of them, the major arms
    // the brighter
    float arm = Arms(g, 0.0, uGalaxy.arms.w, uGalaxy.shape.x);
    float armWide = Arms(g, 0.0, 1.6 * uGalaxy.arms.w, uGalaxy.shape.x);

    // The old disc, slightly denser along the arms (most of all the major arms), and bluer outside
    // the bar, where stars are still forming. A faint, thicker disc reaches further out.
    float edge = smoothstep(uGalaxy.centre.w, 0.85 * uGalaxy.centre.w, g.r);
    float discProfile = exp(-g.r / uGalaxy.disc.x) * mix(0.6, 1.0, smoothstep(0.3 * r0, r0, g.r)) * edge;
    float disc = discProfile * (1.0 + armWide * g.envelope) * Vertical(p.y, uGalaxy.disc.y) +
                 0.12 * exp(-g.r / (2.0 * uGalaxy.disc.x)) * smoothstep(uGalaxy.centre.w, 0.7 * uGalaxy.centre.w, g.r) * Vertical(p.y, 3.0 * uGalaxy.disc.y);
    vec3 discColour = mix(uGalaxy.discColour.rgb, uGalaxy.youngColour.rgb, 0.5 * smoothstep(r0, 2.5 * r0, g.r));
    light += discColour * uGalaxy.glow.y * disc;

    // Young stars and nebulae, close to the plane along the arms, drawn out into streaks
    float ring = Ring(g.r);
    float young = ((0.85 * arm + 0.15 * armWide) * YoungEnvelope(g.r, g.envelope) + ring) * Vertical(p.y, uGalaxy.disc.z);
    if (young > 1e-8)
    {
        // Streaks along the arms, and stretches of them brighter than others
        float streaks = (0.4 + 1.2 * g.filaments.g) * (0.4 + 1.2 * g.clumps);
        light += uGalaxy.youngColour.rgb * uGalaxy.glow.z * young * streaks;
        // A faint glow of ionised gas along the inner edges, where the H II regions are
        float innerEdge = Arms(g, 0.5 * uGalaxy.dust.y, 0.6 * uGalaxy.arms.w, 1.0);
        float nebula = (innerEdge + ring) * smoothstep(0.45, 0.85, g.filaments.b) * smoothstep(0.3, 0.7, g.clumps);
        light += uGalaxy.nebulaColour.rgb * (0.4 * uGalaxy.glow.w) * Vertical(p.y, uGalaxy.disc.z) * g.envelope * nebula;
    }
    return light;
}

// The light of the companions along a ray, split into what is in front of the disc's plane and
// what is behind it (to be dimmed by the disc's dust). Each is three nested Gaussians, integrated
// along the ray exactly, and widened to at least `footprint` (radians) so none fall between pixels.
void CompanionLight(vec3 from, vec3 direction, float footprint, out vec3 front, out vec3 behind)
{
    front = vec3(0.0);
    behind = vec3(0.0);
    // Where the ray crosses the plane (behind the camera, or never, counts as in front of it)
    float tPlane = abs(direction.y) > 1e-6 ? -from.y / direction.y : -1.0;
    const vec3 SCALES = vec3(1.0, 0.3, 0.08);  // Of the halo, core and nucleus
    const vec3 SHARES = vec3(0.6, 0.3, 0.1);   // Of the light
    int count = int(uGalaxy.shape.z);
    for (int i = 0; i < MAX_COMPANIONS; i++)
    {
        if (i >= count)
            break;
        vec3 offset = from - uGalaxy.companionCentre[i].xyz;
        float t = -dot(offset, direction);
        if (t <= 0.0)
            continue;
        float blur = Square(footprint * t);
        float light = 0.0;
        for (int k = 0; k < 3; k++)
        {
            // Covariance sigma^2 I + u u^T: its inverse is (I - u u^T / (sigma^2 + u.u)) / sigma^2
            float sigma2 = Square(uGalaxy.companionCentre[i].w * SCALES[k]) + blur;
            vec3 u = uGalaxy.companionStretch[i].xyz * SCALES[k];
            float stretched = sigma2 + dot(u, u);
            vec3 inverseDirection = (direction - u * (dot(u, direction) / stretched)) / sigma2;
            vec3 inverseOffset = (offset - u * (dot(u, offset) / stretched)) / sigma2;
            float a = dot(direction, inverseDirection);
            float b = dot(direction, inverseOffset);
            float c = dot(offset, inverseOffset);
            // The line integral of the normalised Gaussian
            light += SHARES[k] * (k == 0 ? 1.0 : uGalaxy.companionColour[i].a) * exp(-0.5 * max(c - b * b / a, 0.0)) / (TAU * sigma2 * sqrt(a * stretched));
        }
        vec3 radiance = uGalaxy.companionColour[i].rgb * uGalaxy.companionStretch[i].w * light;
        if (tPlane > 0.0 && t > tPlane)
            behind += radiance;
        else
            front += radiance;
    }
}

// Clips the segment from + t * direction, t in [tNear, tFar], to the galaxy's bounds: a cylinder of
// the disc's radius, `halfThickness` either side of the plane. Returns false if nothing is left.
bool ClipToGalaxy(vec3 from, vec3 direction, float halfThickness, inout float tNear, inout float tFar)
{
    if (abs(direction.y) > 1e-7)
    {
        float t1 = (-halfThickness - from.y) / direction.y;
        float t2 = (halfThickness - from.y) / direction.y;
        tNear = max(tNear, min(t1, t2));
        tFar = min(tFar, max(t1, t2));
    }
    else if (abs(from.y) > halfThickness)
    {
        return false;
    }

    float a = dot(direction.xz, direction.xz);
    float c = dot(from.xz, from.xz) - uGalaxy.centre.w * uGalaxy.centre.w;
    if (a > 1e-12)
    {
        float b = dot(from.xz, direction.xz);
        float discriminant = b * b - a * c;
        if (discriminant < 0.0)
            return false;
        float root = sqrt(discriminant);
        tNear = max(tNear, (-b - root) / a);
        tFar = min(tFar, (-b + root) / a);
    }
    else if (c > 0.0)
    {
        return false;
    }
    return tFar > tNear;
}

#endif // SV_GALAXY_GLSL
