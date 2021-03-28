#version 150

in vec2 textureCoords;
//
out vec4 fragColour;
//
uniform sampler2D texture1;

void main(void)
{
fragColour = texture(texture1, textureCoords);
}