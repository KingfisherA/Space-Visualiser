#version 150

//
in vec3 fVertex;
in vec3 lightVector;
in vec2 textureCoord;
//
out vec4 fragColour;
//
uniform vec3 spherePosition = vec3(0.0f, 0.0f, 0.0f);
uniform float sphereRadius = 1.0f;
uniform float shadowSoftnessU = 2.0f;
//
uniform sampler2D diffuseMap;
//
float SoftSphereShadow(vec3 origin, vec3 direction, vec4 sphereInformation, float shadowSoftness)
{
	float radius2  = sphereInformation.w * sphereInformation.w;
	vec3 L = sphereInformation.xyz - origin;
	float tca = dot(L, direction);
	float d2 = dot(L, L) - tca * tca;
	if(d2 > radius2) 
	{
		return 1.0f;
	}
	float thc = sqrt(radius2 - d2);
	float t0 = tca - thc;
	float t1 = tca + thc;
	if(t0 > t1 && t1 > 0.0f)
	{
		float val = t0;
		t0 = t1;
		t1 = val;
	}

	return (t0 < 0.0) ? 1.0 : smoothstep(1.0, 0.0, 2.5 * shadowSoftness * thc / t0 );
}
//
const float minBrightness = 0.1f;
//

void main(void)
{
	vec3 ray = normalize(lightVector);
	float shadow = max(SoftSphereShadow(fVertex, ray, 
		vec4(spherePosition, sphereRadius), 
		shadowSoftnessU), minBrightness);

	fragColour = texture(diffuseMap, textureCoord);
	fragColour.rgb *= shadow;
}