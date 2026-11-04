#ifndef SV_UTILITIES_GLSL
#define SV_UTILITIES_GLSL

//Utilities
//Various GLSL functions for easier GLSL development

//
float lengthSquared(vec2 vector)
{
	return (vector.x * vector.x) + (vector.y * vector.y);
}
//
float lengthSquared(vec3 vector)
{
	return (vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z);
}
//
float lengthSquared(vec4 vector)
{
	return (vector.x * vector.x) + (vector.y * vector.y) + (vector.z * vector.z) + (vector.w * vector.w);
}
//

#endif // SV_UTILITIES_GLSL
