#include"SV/Precompiled Global.hpp"

#include"Shadows.hpp"
#include"SV/OpenGL/ShaderProgram.hpp"

namespace SV
{
	//
	//
#pragma region Shadow Map Shader Program
//
	const StringView SHADOW_MAP_VERTEX_SHADER = ".\\Shaders\\Shadows\\Shadow Map.v.glsl";
	const StringView SHADOW_MAP_FRAGMENT_SHADER = ".\\Shaders\\Shadows\\Shadow Map.f.glsl";
	ShadowMapShader::ShadowMapShader() : ShaderProgram(VertexShader(SHADOW_MAP_VERTEX_SHADER), FragmentShader(SHADOW_MAP_FRAGMENT_SHADER), AttributeList{ {0, "position"} })
	{
		depthMVPUni = GetUniformLocation("depthMVP");
		textureSampleUni = GetUniformLocation("textureSample");
	}
	//
	glm::mat4 ShadowMapShader::CalculateDepthVP(const glm::vec3& lightPosition)
	{
		//Check this later
		const unsigned short projSize = 10;
		glm::vec3 invLightDirection = glm::vec3(lightPosition.x, lightPosition.y, lightPosition.z);
		glm::mat4 depthProjectionMatrix = glm::ortho<float>(-projSize, projSize, -projSize, projSize, -projSize, 20);
		glm::mat4 depthViewMatrix = glm::lookAt(invLightDirection, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		return depthProjectionMatrix * depthViewMatrix;
	}
	//
#pragma endregion
//
#pragma region Shadow Object Shader Program
//
	const StringView SHADOW_OBJ_VERTEX_SHADER = ".\\Shaders\\Shadows\\Shadow Object.v.glsl";
	const StringView SHADOW_OBJ_FRAGMENT_SHADER = ".\\Shaders\\Shadows\\Shadow Object.f.glsl";
	ShadowObjectShader::ShadowObjectShader() : ShaderProgram(VertexShader(SHADOW_OBJ_VERTEX_SHADER), FragmentShader(SHADOW_OBJ_FRAGMENT_SHADER), AttributeList{ {0, "in_position"}, {1, "in_normal"}, {2, "in_texCoords"} })
	{
		transformMatUni = GetUniformLocation("transformMatrix");
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		normalMatUni = GetUniformLocation("normalMatrix");
		//
		lightPositionUni = GetUniformLocation("lightPosition");
		textureSampUni = GetUniformLocation("textureSample");
		//
		shadowMapUni = GetUniformLocation("shadowMap");
		shadowMapSizeUni = GetUniformLocation("shadowMapSize");
		depthVPUni = GetUniformLocation("depthVP");
	}
	//
#pragma endregion
//
#pragma region Shadow FBO
//
	const glm::mat4 ShadowFBO::biasMatrix(
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.0,
		0.5, 0.5, 0.5, 1.0);
	//
	ShadowFBO::ShadowFBO(unsigned int fboSizeIn) : FBO(), depthVPMat(1.0f)
	{
		this->fboSize = fboSizeIn;

		this->Bind();
		this->AddDepthAttachment(create <SV::DepthTexture2D>(fboSize, fboSize, DepthFormat::Depth16BitFloat));
		this->DrawBuffers({ -1 });
		this->Unbind();

		mapProgram = new ShadowMapShader();
		objProgram = new ShadowObjectShader();

		objProgram->Bind();
		objProgram->BindShadowMapSize(fboSize);
		objProgram->Unbind();
	}
	//
	void ShadowFBO::MapBindShadowRendering(const glm::vec4& clearColour, const glm::vec3& lightPosition)
	{
		Bind();
		glViewport(0, 0, fboSize, fboSize);
		glClearColor(clearColour.r, clearColour.g, clearColour.b, clearColour.a);
		glClear(GL_DEPTH_BUFFER_BIT);

		RecalculateDepthVPMatrix(lightPosition);
		mapProgram->Bind();
	}
	//
	void ShadowFBO::ObjBindShadowRendering(const glm::vec3& lightPosition)
	{
		Unbind();
		objProgram->Bind();


		objProgram->BindShadowMapUnit(0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->GetDepthTexture()->GetTextureID());

		depthVPMat = biasMatrix * depthVPMat;
		objProgram->BindDepthVPMatrix(depthVPMat);
	}
	//
	void ShadowFBO::ShadowRenderingUnbind()
	{
		FBO::Unbind();
		ShaderProgram::Unbind();
	}
	//
	glm::mat4 ShadowFBO::CalculateDepthVP(const glm::vec3& lightPosition)
	{
		//Check this later
		const unsigned short projSize = 10;
		glm::vec3 invLightDirection = glm::vec3(lightPosition.x, lightPosition.y, lightPosition.z);
		glm::mat4 depthProjectionMatrix = glm::ortho<float>(-projSize, projSize, -projSize, projSize, -projSize, 20);
		glm::mat4 depthViewMatrix = glm::lookAt(invLightDirection, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		return depthProjectionMatrix * depthViewMatrix;
	}
	//
#pragma endregion
//
#pragma region Sphere Shadow Shader
//
	const StringView SPHERE_SHADOW_VERTEX_SHADER = ".\\Shaders\\Miscellaneous\\Sphere Soft Shadow\\Sphere Soft Shadow Texture.v.glsl";
	const StringView SPHERE_SHADOW_FRAGMENT_SHADER = ".\\Shaders\\Miscellaneous\\Sphere Soft Shadow\\Sphere Soft Shadow Texture.f.glsl";
	SphereShadowShader::SphereShadowShader() : ShaderProgram(VertexShader(SPHERE_SHADOW_VERTEX_SHADER), FragmentShader(SPHERE_SHADOW_FRAGMENT_SHADER), AttributeList{ {0, "vertex"}, {2, "texCoord"} })
	{
		transformMatUni = GetUniformLocation("transformationMatrix");
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		//
		diffuseMapUni = GetUniformLocation("diffuseMap");
		//
		lightPositionUni = GetUniformLocation("lightPosition");
		//
		spherePosUni = GetUniformLocation("spherePosition");
		sphereRadUni = GetUniformLocation("sphereRadius");
		shadowSoftnessUni = GetUniformLocation("shadowSoftnessU");
	}
	//
#pragma endregion
//
}