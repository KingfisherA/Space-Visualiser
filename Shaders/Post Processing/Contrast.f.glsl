#version 150

in vec2 textureCoordinate;
//
out vec4 fragColour;
//
uniform sampler2D texture1;
//
uniform float contrast;

void main(void)
{
  fragColour = texture(texture1, textureCoordinate);
  fragColour.rgb = (fragColour.rgb - 0.5f) * (1.0f + contrast) + 0.5f;
}