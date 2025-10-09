#include"SV-Engine/Precompiled Global.hpp"
#include"Post Processing.hpp"
#include"SV-Engine/Utilities/String.hpp"

namespace SV
{
	//
	//
#pragma region Gaussian Blur Shader
//
	const StringView BLUR_VERTEX_SHADER = "./Shaders/Post Processing/Blur.v.glsl";
	const StringView BLUR_FRAGMENT_SHADER = "./Shaders/Post Processing/Blur.f.glsl";
	GaussianBlurShader::GaussianBlurShader() : ShaderProgram(VertexShader(BLUR_VERTEX_SHADER), FragmentShader(BLUR_FRAGMENT_SHADER), AttributeList{ {0, "position"} })
	{
		offsetUni = GetUniformLocation("offset");
		sceneTexUni = GetUniformLocation("sceneTexture");
	}
	//
#pragma endregion
//
}
