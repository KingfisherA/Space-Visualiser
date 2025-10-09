//
uniform vec4 ringsParams = vec4(1.4289f, 3.0f, 1.0f / 0.8f, 0.0f); // (inv rings width, rings inner radius, inv rings density, face)

void RingShadowVertex(vec3 position, vec3 lightPosition, inout float ringShadowCoords,
					  inout float ringShadowFade, inout vec2 test)
{
	if (ringsParams.x == 0.0f)
	{
		ringShadowFade = 0.0f;
		return;
	}

	float sina = 0.0f;
	vec2 shadowProj = vec2(0.0f);
	vec3 lightPos = normalize(lightPosition - position.xyz);

	if (ringsParams. w== 0.0f)
	{
		shadowProj = position.xz - lightPos.xz * min(position.y / lightPos.y, 0.0);
		test = shadowProj;
		ringShadowCoords = (length(shadowProj) - ringsParams.x) * ringsParams.y; //ringShadowCoords.x = (length(shadowProj) -(1/(innerWidth))) * innerRadius;
		//
		sina = abs(lightPos.y) * sqrt(dot(lightPos, lightPos));
		ringShadowFade = ringsParams.z * smoothstep(0.0f, 0.001f, sina);
	}
	else
	{
		shadowProj = position.xy - lightPos.xy * min(position.z / lightPos.z, 0.0);
		ringShadowCoords = (length(shadowProj) - ringsParams.x) * ringsParams.y; //ringShadowCoords.x = (length(shadowProj) - ringsParams.y) * ringsParams.x;
		//
		sina = abs(lightPosition.z) * inversesqrt(dot(lightPosition, lightPosition));
		ringShadowFade = ringsParams.z * smoothstep(0.0003, 0.001, sina);
	}
}