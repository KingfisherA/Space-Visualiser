#version 460

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
layout(location = 0) in vec3 a_vertex;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in mat4 a_TransformMatrix; //Requires 4 attributes
layout (location = 6) in mat3 a_NormalMatrix;    //Requires 3 attributes
layout(location = 9) in vec3 a_starPropeties; //Radius, Star Temperature, Noise Octaves
layout(location = 10) in vec3 a_starColour;
//
uniform mat4 u_projectionMatrix;
uniform mat4 u_viewMatrix;
//
uniform vec3 u_cameraPosition;
//
out vec3 v_Vertex;
out vec3 v_CamVector;
out vec3 v_NormalVector;
out vec3 v_StarPropeties; //Radius, Star Temperature, Noise Octaves
out vec3 v_StarColour;
//

void main()
{
	vec4 vertexWorld = a_TransformMatrix * vec4(a_vertex, 1.0f);
	v_Vertex = a_vertex;

	v_NormalVector = a_NormalMatrix * a_normal;
	v_CamVector = (u_cameraPosition - vertexWorld.xyz);

	v_StarPropeties = a_starPropeties;
	v_StarColour = a_starColour;

	gl_Position = u_projectionMatrix * u_viewMatrix * vertexWorld;
}