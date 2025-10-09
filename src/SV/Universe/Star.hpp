#pragma once
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/OpenGL/ShaderProgram.hpp"
#include"SV-Engine/Scene/Transformation.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Utilities/SVMemory.hpp"
#include"CelestrialBody.hpp"

namespace SV
{
	//Forward declarations
	class VAO;
	class ArrayBuffer;
	class Camera;

	//
	class StarShader : public ShaderProgram
	{
	public:
		StarShader();
		//
		inline void SetViewMatrix(const glm::mat4& va) { UploadMatrixF(viewMatLocation, va); }
		inline void SetProjectionMatrix(const glm::mat4& va) { UploadMatrixF(projMatLocation, va); }
		//
		inline void SetTime(const float va) { UploadFloat(timeLocation, va); }
		inline void SetCamPosition(const glm::vec3& va) { UploadFloat(cameraPositionLocation, va); }
		//
	private:
		//
		Uniform viewMatLocation = 0;
		Uniform projMatLocation = 0;
		//
		Uniform timeLocation = 0;
		Uniform cameraPositionLocation = 0;
	};

	//
	class CoronaShader : public ShaderProgram
	{
	public:
		CoronaShader();
		//
		inline void SetViewMatrix(const glm::mat4& va) { UploadMatrixF(viewMatLocation, va); }
		inline void SetProjectionMatrix(const glm::mat4& va) { UploadMatrixF(projMatLocation, va); }
		//
		inline void SetTime(const float va) { UploadFloat(timeLocation, va); }
		inline void SetNoiseOctaves(const GLsizei va) { UploadUInt(noiseOctavesLocation, va); }
		//
	private:
		//
		Uniform viewMatLocation = 0;
		Uniform projMatLocation = 0;
		//
		Uniform timeLocation = 0;
		Uniform noiseOctavesLocation = 0;
	};

	//
	class Star : public CelestrialBody
	{
	public:
		Star(const double massIn, float radiusIn, float starTempIn) :
			CelestrialBody(CelestrialBodyType::Star, massIn),
			starColour(1.0f),
			radius(radiusIn),
			starTemperature(starTempIn)
		{
		}

		Star(const double massIn, float radiusIn, float starTempIn, const glm::vec3& starColIn) :
			CelestrialBody(CelestrialBodyType::Star, massIn),
			radius(radiusIn),
			starTemperature(starTempIn),
			starColour(starColIn)
		{
		}

		Star(const double massIn, float radiusIn, float starTempIn, const glm::vec3& starColIn, const glm::vec3& position) :
			CelestrialBody(CelestrialBodyType::Star, massIn),
			radius(radiusIn),
			starTemperature(starTempIn),
			starColour(starColIn)
		{
		}

		Star(const Star& s) :
			CelestrialBody(CelestrialBodyType::Star, s.mass),
			radius(s.radius),
			starTemperature(s.starTemperature),
			starColour(s.starColour)
		{
		}
		//
		float radius = 1.0f;
		float starTemperature = 5800.0f;
		glm::vec3 starColour;
		//
	private:
		//
		friend class StarRenderer;
		//
	};

	namespace PresetStars
	{
		class Sun : public Star
		{
		public:
			Sun() : Star(1.989E30, 1.0f, 5800.0f, glm::vec3(1.0f))
			{
			}
			Sun(const glm::vec3& posIn) : Star(1.989E30, 1.0f, 5800.0f, glm::vec3(1.0f), posIn)
			{
			}
		};
	}

	//
	class StarRenderer : public CelestrialRenderer<SV::Star>
	{
	public:
		StarRenderer();
		//
		void Render(float time, const Camera& currentCamera, const glm::mat4& projMatrix);
		//
	private:
		//
		void PackStarAttributes(SV::Array<float>& data);
		void PackCoronaAttributes(SV::Array<float>& data, const Camera& currentCamera);
		//
		SV::Array<Ref<SV::Star>>& stars = CelestrialRenderer<SV::Star>::celestrialBodies;
		SV::Array<float> starAttributeArray;
		//
		Ref<StarShader> starProgram;
		Ref<CoronaShader> coronaProgram;
		//
		static constexpr unsigned short starAttributeSize = 31U;
		Ref<ArrayBuffer> starAttributeBuffer;
		//
		static constexpr unsigned short coronaAttributeSize = 19U;
		Ref<ArrayBuffer> coronaAttributeBuffer;
		//
		Ref<VAO> starVAO;
		Ref<VAO> coronaVAO;
	};

}
