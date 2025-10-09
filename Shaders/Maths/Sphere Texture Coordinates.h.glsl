#pragma remove
#include"../GLSL Language Specification.h.glsl"
#pragma endremove


vec2 CalculateSphereTexCoords(const vec3 n)
{
    const float PI = 3.141592653589793238f;
    return vec2(
        (atan(n.x, n.z) / PI) / 2.0f + 0.5f,
        (asin(-n.y) / (PI / 2.0f)) / 2.0f + 0.5f
    );
}