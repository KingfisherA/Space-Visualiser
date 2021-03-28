#version 140 
in vec3 position;
in vec2 texCoords;
out vec2 Texcoord;

void main(void) {  
Texcoord = texCoords;
 gl_Position = vec4(position, 1.0);
}