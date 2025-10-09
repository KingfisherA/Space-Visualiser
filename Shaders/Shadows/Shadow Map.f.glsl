#version 330 core

//
out vec4 fragColour;
//
in vec2 textureCoords;
//
uniform sampler2D textureSample;
//
void main()
{
    if(texture(textureSample, textureCoords).a < 0.5f)
	{
        discard;
    }

 fragColour = vec4(1.0f);
}