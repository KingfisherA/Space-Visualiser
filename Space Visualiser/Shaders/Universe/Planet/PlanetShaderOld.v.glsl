#version 120

varying vec3 lightVector;
varying vec3 normalVector;
varying vec3 texCoords;
uniform mat4 transformMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 lightPosition;

void main()
{
//projectionMatrix * viewMatrix * transformMatrix
vec4 position =  gl_ModelViewProjectionMatrix * gl_Vertex;
texCoords = gl_Vertex.xyz;

normalVector  = (transformMatrix *vec4(gl_Normal.xyz, 1.0f)).xyz;
lightVector = (lightPosition - gl_Vertex.xyz);
gl_Position = position;
}