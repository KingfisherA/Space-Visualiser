#version 150 

in vec2 position;
//
uniform vec2 offset;
out vec2 blurTextureCoords[11];
//

void main(void) 
{
    vec2 centreTexCoord = (position*0.5f) + vec2(0.5f);
    for(int i = -5; i < 5; i++)
    {
        blurTextureCoords[i+5] = centreTexCoord + (offset * i);
    }

	gl_Position = vec4(position, 0.0f, 1.0f);
}