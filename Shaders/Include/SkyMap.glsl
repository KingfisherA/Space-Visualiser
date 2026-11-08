#ifndef SV_SKY_MAP_GLSL
#define SV_SKY_MAP_GLSL

// The compact objects' sky, baked once into an octahedral map (Shaders/SkyBake.frag) and looked
// up by the ray tracer (Shaders/CompactObject.frag) wherever a ray escapes. Mirrors SkyMap in
// src/SV/Universe/CompactObject.cpp.
//
// The map is laid out in galactic coordinates: x towards the galactic centre, z the galactic
// north pole, so the Milky Way's band lies along z = 0 (the octahedron's equator, away from the
// map's folded edges). Each texel holds:
//   rgb = sqrt(radiance / SKY_MAP_SCALE): the diffuse sky (the band's glow, dust, nebulae and
//         distant galaxies). The square root spends the 8 bits where the sky is darkest.
//   a   = how crowded the resolved stars drawn over it are (the band's star clouds, thinned by dust)

// Radiance at an encoded 1
const float SKY_MAP_SCALE = 0.4;

// The galactic axes in the objects' frame (y up, the disc in the XZ plane): the galactic centre
// lies behind the object from the first view (the camera starts on +z), and the band crosses the
// view tilted about 30 degrees
const vec3 GALACTIC_X = vec3(-0.240909, 0.115636, -0.963634);
const vec3 GALACTIC_Y = vec3(-0.830799, 0.488705, 0.266344);
const vec3 GALACTIC_Z = vec3(0.501732, 0.864752, -0.021663);

vec3 ToGalactic(vec3 direction)
{
    return vec3(dot(direction, GALACTIC_X), dot(direction, GALACTIC_Y), dot(direction, GALACTIC_Z));
}

vec2 SignNotZero(vec2 v)
{
    return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

// A unit direction (galactic) to its place in the map, in [0, 1]^2: the upper hemisphere (z > 0)
// is the central diamond, the lower folded out over the corners
vec2 OctahedralEncode(vec3 g)
{
    g /= abs(g.x) + abs(g.y) + abs(g.z);
    vec2 p = g.z >= 0.0 ? g.xy : (1.0 - abs(g.yx)) * SignNotZero(g.xy);
    return 0.5 * p + 0.5;
}

vec3 OctahedralDecode(vec2 uv)
{
    vec2 f = 2.0 * uv - 1.0;
    vec3 g = vec3(f, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-g.z, 0.0);
    g.xy -= t * SignNotZero(g.xy);
    return normalize(g);
}

#endif // SV_SKY_MAP_GLSL
