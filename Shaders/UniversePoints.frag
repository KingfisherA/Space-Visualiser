#version 410
#extension GL_ARB_shading_language_420pack : require

layout(location = 0) in vec2 vCorner;
layout(location = 1) flat in vec3 vRadiance;
layout(location = 2) flat in int vKind;

// Added to the scene target (HDR)
layout(location = 0) out vec4 fragColour;

// Mirrors POINT_EXTENT in Shaders/Include/Universe.glsl, and the black hole kinds in UniversePoints.vert
const float POINT_EXTENT = 3.0;
const int KIND_STELLAR_BLACK_HOLE = 6;
const int KIND_SUPERMASSIVE_BLACK_HOLE = 7;

void main()
{
    // A Gaussian, lowered to reach 0 at the quad's inscribed circle
    float r2 = dot(vCorner, vCorner) * POINT_EXTENT * POINT_EXTENT;
    float edge = exp(-0.5 * POINT_EXTENT * POINT_EXTENT);
    float profile = max(exp(-0.5 * r2) - edge, 0.0) / (1.0 - edge);
    if (vKind == KIND_SUPERMASSIVE_BLACK_HOLE || vKind == KIND_STELLAR_BLACK_HOLE)
    {
        // A ring (as the light bent around a black hole's shadow), the same light in all
        float r = length(vCorner);
        profile = 2.5 * exp(-0.5 * (r - 0.45) * (r - 0.45) / (0.08 * 0.08)) * (1.0 - smoothstep(0.85, 1.0, r));
    }
    fragColour = vec4(vRadiance * profile, 0.0);
}
