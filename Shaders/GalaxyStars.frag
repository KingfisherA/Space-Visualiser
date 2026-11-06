#version 410
#extension GL_ARB_shading_language_420pack : require

layout(location = 0) in vec2 vCorner;
layout(location = 1) flat in vec3 vRadiance;

// Added to the stars target (HDR)
layout(location = 0) out vec4 fragColour;

// Mirrors EXTENT_SIGMAS in GalaxyStars.vert
const float EXTENT_SIGMAS = 3.0;

void main()
{
    // A Gaussian, lowered to reach 0 at the quad's inscribed circle
    float r2 = dot(vCorner, vCorner) * EXTENT_SIGMAS * EXTENT_SIGMAS;
    float edge = exp(-0.5 * EXTENT_SIGMAS * EXTENT_SIGMAS);
    float profile = max(exp(-0.5 * r2) - edge, 0.0) / (1.0 - edge);
    fragColour = vec4(vRadiance * profile, 0.0);
}
