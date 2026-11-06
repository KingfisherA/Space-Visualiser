#ifndef SV_PROCEDURAL_GLSL
#define SV_PROCEDURAL_GLSL

// Procedural noise and a star field for the test scenes' full screen passes

// Mirrors BLACKBODY_TABLE_* in src/SV/Universe/Star.hpp: blackbody colours (luminance 1) at this
// many temperatures, spaced evenly in log temperature between the two (K)
const int BLACKBODY_SAMPLES = 32;
const float BLACKBODY_MIN = 1000.0;
const float BLACKBODY_MAX = 40000.0;

// Where a temperature falls in the blackbody table (fractional index)
float BlackbodyIndex(float temperature)
{
    return clamp(log(temperature / BLACKBODY_MIN) / log(BLACKBODY_MAX / BLACKBODY_MIN), 0.0, 1.0) * float(BLACKBODY_SAMPLES - 1);
}

float Hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Value noise in [0, 1]
float Noise(vec3 x)
{
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(Hash(i), Hash(i + vec3(1, 0, 0)), f.x),
                   mix(Hash(i + vec3(0, 1, 0)), Hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(Hash(i + vec3(0, 0, 1)), Hash(i + vec3(1, 0, 1)), f.x),
                   mix(Hash(i + vec3(0, 1, 1)), Hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// Four octaves of value noise, in [0, 1]
float Fbm(vec3 p)
{
    float sum = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++)
    {
        sum += amplitude * Noise(p);
        p = p * 2.03 + vec3(7.1, 3.7, 1.3);
        amplitude *= 0.5;
    }
    return sum / 0.9375;
}

// Interleaved gradient noise (Jimenez 2014): offsets each pixel's samples, trading banding for fine noise
float Dither(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

// The colour (luminance 1) of a star at a temperature (K): defined by the shader including this
vec3 StarColour(float temperature);

// The stars seen in a direction, each a Gaussian about a pixel (pixelAngle, radians) across: two
// layers of stars, each star in a cell of a grid around the sky's sphere (only those near the
// sphere, so the eight cells around the ray find all the stars near it). Most are faint, a few bright.
vec3 StarField(vec3 direction, float pixelAngle)
{
    vec3 colour = vec3(0.0);
    float width = max(pixelAngle, 1e-5);
    for (int layer = 0; layer < 2; layer++)
    {
        float cellsPerRadian = layer == 0 ? 60.0 : 170.0;
        float chance = layer == 0 ? 0.12 : 0.035;
        vec3 p = direction * cellsPerRadian;
        vec3 base = floor(p - 0.5);
        for (int c = 0; c < 8; c++)
        {
            vec3 cell = base + vec3(c & 1, (c >> 1) & 1, c >> 2) + float(layer) * 211.0;
            if (Hash(cell) > chance)
                continue;
            vec3 star = cell - float(layer) * 211.0 + vec3(Hash(cell + 11.3), Hash(cell + 23.9), Hash(cell + 37.1));
            if (abs(length(star) - cellsPerRadian) > 0.5)
                continue;
            float distance = length(direction - normalize(star));
            float brightness = 0.012 + 2.5 * pow(Hash(cell + 51.7), 18.0);
            float temperature = mix(3200.0, 12000.0, Hash(cell + 63.3));
            colour += brightness * StarColour(temperature) * exp(-(distance * distance) / (width * width));
        }
    }
    return colour;
}

#endif
