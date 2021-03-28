//
uniform sampler2D ringsMap;

float RingShadowFragment(float ambient, float ringShadowFade, float ringShadowCoords)
{
if(ringShadowFade == 0.0f) return 1.0f;

	float ringAlpha = 1.0f - ringShadowFade * texture(ringsMap, vec2(ringShadowCoords, 0.75)).a;
	float ringShadow = clamp(ringAlpha, ambient, 1.0f);

	return ringShadow;
}