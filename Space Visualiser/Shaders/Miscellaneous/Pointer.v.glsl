#version 150

in vec2 position;
//
out vec2 textureCoordinate;
//
uniform mat4 scaleMatrix;
uniform mat4 viewProjectionMatrix;

void main(void) 
{  
vec4 positionFinal = scaleMatrix * vec4(position, 0.0f, 1.0f);
textureCoordinate = (positionFinal.xy / positionFinal.w) / 2 + 0.5;
gl_Position = positionFinal;
}