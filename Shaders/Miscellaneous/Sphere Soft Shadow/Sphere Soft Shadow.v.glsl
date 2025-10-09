#version 150

//
in vec3 vertex;
//
out vec3 fVertex;
out vec3 lightVector;
//
uniform mat4 transformationMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
//
uniform vec3 lightPosition = vec3(0,0,0);
//

void main(void)
{
	vec4 positionWorld = transformationMatrix * vec4(vertex, 1.0f);

	fVertex = positionWorld.xyz;

	lightVector = (lightPosition - positionWorld.xyz);

	gl_Position = projectionMatrix * viewMatrix * positionWorld;
}