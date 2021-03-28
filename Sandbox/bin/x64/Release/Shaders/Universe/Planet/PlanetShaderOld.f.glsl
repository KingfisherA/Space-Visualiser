#version 120

varying vec3 lightVector;
varying vec3 normalVector;
varying vec3 texCoords;
uniform sampler2D textureSample;
uniform vec3 lightColour;

void main()
{
   vec2 textureCoords = vec2((atan(texCoords.y, texCoords.x) / 3.1415926 + 1.0) * 0.5,(asin(texCoords.z) / 3.1415926 + 0.5));

	  vec3 unitNormal = normalize(normalVector);
	  vec3 unitLightVector = normalize(lightVector);

	  float dotResult = dot(unitNormal, unitLightVector);
	  float brightness = max(dotResult, 0.1);
	  vec3 DiffuseColour = brightness*lightColour;

 gl_FragColor = (vec4(DiffuseColour, 1.0f) * texture2D(textureSample, textureCoords));
//gl_FragColor = vec4(normalVector.xyz, 1.0f);
}