#pragma once
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/OpenGL/OpenGL Context.hpp"
#include"SV-Engine/OpenGL/ShaderProgram.hpp"
#include"SV-Engine/OpenGL/FBO.hpp"

typedef unsigned int TextureUnits;

namespace SV
{
	//
	class ShadowMapShader : public ShaderProgram
	{
	public:
		ShadowMapShader();
		//
		inline void BindDepthMVPMatrix(const glm::mat4& value) { UploadMatrixF(depthMVPUni, value); }
		inline void BindTextureUnit(TextureUnits unit) { UploadInt(textureSampleUni, int(unit)); }
		//
		static glm::mat4 CalculateDepthVP(const glm::vec3& lightPosition);
		// 
	private:
		Uniform depthMVPUni = 0;
		Uniform textureSampleUni = 0;
	};

	//
	class ShadowObjectShader : public ShaderProgram
	{
	public:
		ShadowObjectShader();
		//
		inline void BindTransformMatrix(const glm::mat4& value) { UploadMatrixF(transformMatUni, value); }
		inline void BindViewMatrix(const glm::mat4& value) { UploadMatrixF(viewMatUni, value); }
		inline void BindProjectionMatrix(const glm::mat4& value) { UploadMatrixF(projMatUni, value); }
		inline void BindNormalMatrix(const glm::mat3& value) { UploadMatrixF(normalMatUni, value); }
		//
		inline void BindLightPosition(const glm::vec3& value) { UploadFloat(lightPositionUni, value); }
		inline void BindTextureSampUnit(TextureUnits unit) { UploadInt(textureSampUni, (int)unit); }
		//
		inline void BindShadowMapUnit(const TextureUnits value) { UploadInt(shadowMapUni, (const int)value); }
		inline void BindShadowMapSize(const unsigned int size) { UploadInt(shadowMapSizeUni, (const int)size); }
		inline void BindDepthVPMatrix(const glm::mat4& value) { UploadMatrixF(depthVPUni, value); }
		//
	private:
		Uniform transformMatUni = 0;
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		Uniform normalMatUni = 0;
		//
		Uniform lightPositionUni = 0;
		Uniform textureSampUni = 0;
		//
		Uniform shadowMapUni = 0;
		Uniform shadowMapSizeUni = 0;
		Uniform depthVPUni = 0;
	};

	//
	class ShadowFBO : public FBO
	{
	public:
		ShadowFBO(unsigned int fboSizeIn);
		~ShadowFBO()
		{
			delete mapProgram;
			delete objProgram;
		}
		//
		void MapBindShadowRendering(const glm::vec4& clearColour, const glm::vec3& lightPosition);
		inline void MapBindDepthMVP(const glm::mat4& modelMatrix) { mapProgram->BindDepthMVPMatrix(depthVPMat * modelMatrix); }
		inline void MapBindTextureUnit(TextureUnits unit) { mapProgram->BindTextureUnit(unit); }
		//
		void ObjBindShadowRendering(const glm::vec3& lightPosition);
		//
		inline void ObjBindLightPosition(const glm::vec3& lightPosition) { objProgram->BindLightPosition(lightPosition); }
		inline void ObjBindTextureUnit(TextureUnits unit) { objProgram->BindTextureSampUnit(unit); }
		//
		inline void ObjBindViewMatrix(const glm::mat4& value) { objProgram->BindViewMatrix(value); }
		inline void ObjBindProjectionMatrix(const glm::mat4& value) { objProgram->BindProjectionMatrix(value); }
		inline void ObjBindTranformationMatrix(const glm::mat4& value) { objProgram->BindTransformMatrix(value); }
		inline void ObjBindNormalMatrix(const glm::mat3& value) { objProgram->BindNormalMatrix(value); }
		//
		void ShadowRenderingUnbind();
		//
		static glm::mat4 CalculateDepthVP(const glm::vec3& lightPosition);
		inline void RecalculateDepthVPMatrix(const glm::vec3& lightPosition) { depthVPMat = CalculateDepthVP(lightPosition); }
		//
		inline ShadowMapShader* GetShadowMapProgram() { return mapProgram; }
		inline ShadowObjectShader* GetShadowObjProgram() { return objProgram; }
		inline glm::mat4 GetDepthVPMatrix() { return depthVPMat; }
		//
		const static glm::mat4 biasMatrix;
		//
	protected:
		//
		using FBO::AddColourAttachment;
		using FBO::AddDepthAttachment;
		using FBO::DrawBuffers;
		using FBO::ReadBuffer;
		//
	private:
		//
		glm::mat4 depthVPMat;
		//
		ShadowMapShader* mapProgram = nullptr;
		ShadowObjectShader* objProgram = nullptr;
		//
		unsigned int fboSize = 0;
	};

	//
	class SphereShadowShader : public ShaderProgram
	{
	public:
		SphereShadowShader();
		//
		inline void BindTransformMatrix(const glm::mat4& value) { UploadMatrixF(transformMatUni, value); }
		inline void BindViewMatrix(const glm::mat4& value) { UploadMatrixF(viewMatUni, value); }
		inline void BindProjectionMatrix(const glm::mat4& value) { UploadMatrixF(projMatUni, value); }
		//
		inline void BindDiffuseMapUnit(TextureUnits unit) { UploadInt(diffuseMapUni, int(unit)); }
		//
		inline void BindLightPosition(const glm::vec3& value) { UploadFloat(lightPositionUni, value); }
		//
		inline void BindSpherePos(const glm::vec3& pos) { UploadFloat(spherePosUni, pos); }
		inline void BindSphereRad(float radius) { UploadFloat(sphereRadUni, radius); }
		inline void BindShadowSofteness(float softness) { UploadFloat(shadowSoftnessUni, softness); }
		//
	private:
		//
		Uniform transformMatUni = 0;
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		//
		Uniform diffuseMapUni = 0;
		//
		Uniform lightPositionUni = 0;
		//
		Uniform spherePosUni = 0;
		Uniform sphereRadUni = 0;
		Uniform shadowSoftnessUni = 0;
	};
}