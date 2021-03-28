#version 150

in vec2 position;
//
out vec2 textureCoordinates;

void main(void) 
{  
textureCoordinates = (position*0.5f) + vec2(0.5f);
 gl_Position = vec4(position, 0.0f, 1.0f);
}