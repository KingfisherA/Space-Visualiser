#version 150

in vec2 textureCoordinates;
//
out vec4 fragColour;
//
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;

void main(void)
{
fragColour = texture(texture1, textureCoordinates) + 
  mix(texture(texture2, textureCoordinates), 
  texture(texture3, textureCoordinates), 0.6f);
}