#version 150

//
in vec3 vertex;
//
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformationMatrix;
//
out vec2 textureCoordinate;
out vec3 fVertex;
//

void main(void)
{
textureCoordinate = (vertex.xy * 0.5f) + vec2(0.5f);
fVertex = vertex;
 gl_Position = projectionMatrix*viewMatrix*transformationMatrix*vec4(vertex, 1.0f);
}