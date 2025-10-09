//Provides functions for creating scale matricies

mat4 ScaleMatrix(float x, float y, float z)
{
	return mat4(x, 0, 0, 0,
						  0, y,0, 0,
						  0,0, z, 0,
						  0,0,0,1);
}
//
mat4 ScaleMatrix(float s)
{
	return ScaleMatrix(s, s, s);
}
uniform