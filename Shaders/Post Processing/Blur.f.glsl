#version 150

out vec4 fragColour;
//
in vec2 blurTextureCoords[11];
//
uniform sampler2D sceneTexture;

void main(void)
{

    fragColour = vec4(0.0);
    fragColour += texture(sceneTexture, blurTextureCoords[0]) * 0.0093;
    fragColour += texture(sceneTexture, blurTextureCoords[1]) * 0.028002;
    fragColour += texture(sceneTexture, blurTextureCoords[2]) * 0.065984;
    fragColour += texture(sceneTexture, blurTextureCoords[3]) * 0.121703;
    fragColour += texture(sceneTexture, blurTextureCoords[4]) * 0.175713;
    fragColour += texture(sceneTexture, blurTextureCoords[5]) * 0.198596;
    fragColour += texture(sceneTexture, blurTextureCoords[6]) * 0.175713;
    fragColour += texture(sceneTexture, blurTextureCoords[7]) * 0.121703;
    fragColour += texture(sceneTexture, blurTextureCoords[8]) * 0.065984;
    fragColour += texture(sceneTexture, blurTextureCoords[9]) * 0.028002;
    fragColour += texture(sceneTexture, blurTextureCoords[10]) * 0.0093;

}