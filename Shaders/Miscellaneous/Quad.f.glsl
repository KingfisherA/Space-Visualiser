#version 140
in vec2 Texcoord;
uniform sampler2D inTexture;
out vec4 fragColour;

void main(void)
{
   fragColour = texture(inTexture, Texcoord);
}
