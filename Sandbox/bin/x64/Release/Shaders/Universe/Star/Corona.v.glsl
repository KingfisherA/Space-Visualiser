#version 460

#pragma remove
#include ".\Shaders\GLSL Language Specification.h.glsl"
#pragma endremove

//
layout(location = 0) in vec2 a_vertex;
layout(location = 1) in mat4 a_transformationMatrix;
layout(location = 5) in vec3 a_starColour;
//
uniform mat4 u_projectionMatrix;
uniform mat4 u_viewMatrix;
//
out vec3 v_Vertex;
out vec3 v_starColour;
//
const float noiseScale = 1.5f;

const vec2 u_Size = vec2(1.0f, 1.0f);

void main()
{
	v_Vertex = vec3(a_vertex * noiseScale, 0.0f);
	v_starColour = a_starColour;

	vec3 cameraRightWorldspace = vec3(u_viewMatrix[0][0], u_viewMatrix[1][0], u_viewMatrix[2][0]);
	vec3 cameraUpWorldspace = vec3(u_viewMatrix[0][1], u_viewMatrix[1][1], u_viewMatrix[2][1]);

	vec3 vertexPosWorldspace = cameraRightWorldspace * v_Vertex.x * u_Size.x +
							   cameraUpWorldspace * v_Vertex.y * u_Size.y;

	gl_Position = u_projectionMatrix * u_viewMatrix * a_transformationMatrix * vec4(vertexPosWorldspace, 1.0f);
}