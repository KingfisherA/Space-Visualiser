#version 150

in vec2 textureCoordinate;
//
uniform sampler2D textureSample;
//
out vec4 fragColour;

void main(void)
{
vec4 colour =  texture(textureSample, textureCoordinate);
fragColour = vec4(1.0f-colour.rgb, 1.0f);
}