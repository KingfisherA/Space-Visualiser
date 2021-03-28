#version 150

//
in vec3 vertex;
in vec2 texCoord;
//
out vec3 fVertex;
out vec3 lightVector;
out vec2 textureCoord;
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
	textureCoord = texCoord;

	gl_Position = projectionMatrix * viewMatrix * positionWorld;
}