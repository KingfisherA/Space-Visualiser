#version 150

in vec2 Texcoord;
//
out vec4 fragColour;
 //
uniform sampler2D flareTexture;
uniform float brightness;

void main (void) 
{
fragColour = texture(flareTexture, Texcoord);
fragColour.a *= brightness;
}