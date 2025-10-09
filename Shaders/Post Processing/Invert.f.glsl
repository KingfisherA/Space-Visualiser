#version 150

in vec2 textureCoordinate;
//
out vec4 fragColour;
//
uniform sampler2D texture1;

void main(void)
{
  fragColour = vec4(1.0f - texture(texture1, textureCoordinate).rgb, 1.0f);
}