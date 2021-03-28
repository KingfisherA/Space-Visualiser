#version 330 core

in vec4 shadowMapPos;
in vec3 lightVector;
in vec3 normalVector;
in vec2 textureCoords;
//
out vec4 fragColour;
//
uniform sampler2D textureSample;
//
uniform sampler2D shadowMap;
uniform int shadowMapSize;
const float offsetBias = 0.005f;
//
//PCF sampling constants
const int pcfCount = 3;
const int totalTexels = ((pcfCount*2)+1)*((pcfCount*2)+1);
//
//Poisson Sampling
const vec2 poissonDisk[4] = vec2[](
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);
const float invSpread = 1.0f / 2000.0f;
//
//Custom functions
float random(in vec4 seed4)
{
    float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}
//
//
void main()
{
    float texelSize = 1.0f / float(shadowMapSize);
    float total = 0.0f;
    for(int x = -pcfCount; x <= pcfCount; x++)
	{
        for(int y = -pcfCount; y <= pcfCount; y++)
	    {
		    //Poisson Sampling offset
            int randIndex = int(4.0*random(vec4(gl_FragCoord.xyy, x)))%4;
			vec2 pSOffset = vec2(poissonDisk[randIndex] * invSpread);
			//PCF Sampling Offset
			vec2 pcfOffset = vec2(x, y) * texelSize;

            float objectNearestLight = texture(shadowMap, shadowMapPos.xy + pcfOffset + pSOffset).r;
            if ((shadowMapPos.z - offsetBias) > objectNearestLight)
	        {
			    //Sample is in the shadow, so add 1 to the total samples in the shadow
                total++;
            }
	      }
	  }

	total /= totalTexels;

    float lightFactor = 1.0f - total;

	vec3 unitNormal = normalize(normalVector);
	vec3 unitLightVector = normalize(lightVector);

	float brightness = max(dot(unitNormal, unitLightVector) * lightFactor, 0.2);

	vec4 textureColour = texture(textureSample,  textureCoords);
	fragColour = vec4(brightness * textureColour.rgb, textureColour.a);
}