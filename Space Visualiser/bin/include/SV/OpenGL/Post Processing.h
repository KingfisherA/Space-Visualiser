#pragma once
#include"OpenGL Context.h"
#include"ShaderProgram.h"
#include"FBO.h"


//Forward declarations
typedef unsigned int TextureUnits;

namespace SV
{
	//
	class GaussianBlurShader : public ShaderProgram
	{
	public:
		GaussianBlurShader();
		//
		inline void BindOffset(const glm::vec2& offset) { UploadFloat(offsetUni, offset); }
		inline void BindSceneTexUnit(const TextureUnits unit) { UploadInt(sceneTexUni, (const int)unit); }
		//
	private:
		//
		Uniform offsetUni = 0;
		Uniform sceneTexUni = 0;
	};
}