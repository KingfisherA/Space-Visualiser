#pragma once
#include"SV-Engine/OpenGL/OpenGL Context.hpp"
#include"SV-Engine/OpenGL/ShaderProgram.hpp"
#include"SV-Engine/Utilities/SVMemory.hpp"
#include"SV-Engine/Scene/Transformation.hpp"
#include"SV/Atmosphere/Atmosphere.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include"SV-Engine/Scene/Camera.hpp"
#include"CelestrialBody.hpp"

#include<type_traits>
#include<typeinfo>

namespace SV
{
	template<typename A>
	class Array;
	class ITexture;
	class Texture2D;

#pragma region Planets

	//
	class PlanetNew : public CelestrialBody 
	{
	public:
		PlanetNew(const double massIn, const Ref<SV::ITexture>& diffuseTexIn, const Ref<SV::Atmosphere::IAtmosphere>& atmoIn = nullptr, const Ref<SV::ITexture>& specularTexIn = nullptr,
			const Ref<SV::ITexture>& cloudsMapIn = nullptr, const Ref<SV::ITexture>& nightMapIn = nullptr, const Ref<SV::ITexture>& ringsShadowMapIn = nullptr) :
			CelestrialBody(CelestrialBodyType::Planet, massIn),
			diffuseMap(diffuseTexIn),
			specularMap(specularTexIn),
			cloudsMap(cloudsMapIn),
			nightMap(nightMapIn),
			ringShadowMap(ringsShadowMapIn),
			planetAtmo(atmoIn) {}
		virtual ~PlanetNew() {}
		//
		inline const Ref<SV::ITexture>& GetDiffuseMap() const noexcept { return diffuseMap; }
		inline const Ref<SV::ITexture>& GetSpecularMap() const noexcept { return specularMap; }
		inline const Ref<SV::ITexture>& GetCloudsMap() const noexcept { return cloudsMap; }
		inline const Ref<SV::ITexture>& GetNightMap() const noexcept { return nightMap; }
		inline const Ref<SV::ITexture>& GetRingsShadowMap() const noexcept { return ringShadowMap; }
		//
		inline const Ref<SV::Atmosphere::IAtmosphere>& GetAtmosphere() const noexcept { return planetAtmo; }
		inline const SV::Atmosphere::ProcessedAtmoData& GetAtmosphericData() const noexcept
		{
			SV_ASSERT_T(planetAtmo == nullptr, SV::PlanetNew::GetAtmosphericData, "Cannot retrieve atmospheric data as planet's atmosphere is nullptr");
			return planetAtmo->GetAtmosphericData();
		}
		inline const Ref<SV::Atmosphere::AtmospherePrecomputer>& GetPrecomputer() const noexcept
		{
			SV_ASSERT_T(planetAtmo == nullptr, SV::PlanetNew::GetPrecomputer, "Cannot retrieve atmospheric precomputer as planet's atmosphere is nullptr");
			return planetAtmo->GetPrecomputer();
		}
		//
	protected:
		//
		PlanetNew(const double massIn) : CelestrialBody(CelestrialBodyType::Planet, massIn) {}
		PlanetNew(const double massIn, const Ref<SV::Atmosphere::IAtmosphere>& atomIn) :  CelestrialBody(CelestrialBodyType::Planet, massIn), planetAtmo(atomIn) {}
		//
		Ref<SV::ITexture> diffuseMap;
		Ref<SV::ITexture> specularMap;
		Ref<SV::ITexture> cloudsMap;
		Ref<SV::ITexture> nightMap;
		Ref<SV::ITexture> ringShadowMap;
		//
		Ref<SV::Atmosphere::IAtmosphere> planetAtmo;
		//
	};

	//
	class Mercury : public PlanetNew
	{
	public:
		Mercury(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::Texture2D> mercuryDiffuseMap;
		//
		static Ref<SV::Atmosphere::IAtmosphere> mercuryAtmosphere;
	};

	//
	class Venus : public PlanetNew
	{
	public:
		Venus(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::AsyncTexture2D> venusDiffuseMap;
		static Ref<SV::AsyncTexture2D> venusCloudsMap;
		//
		static Ref<SV::Atmosphere::IAtmosphere> venusAtmosphere;
	};

	//
	class Earth : public PlanetNew
	{
	public:
		Earth(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::AsyncTexture2D> earthDiffuseMap;
		static Ref<SV::AsyncTexture2D> earthSpecularMap;
		static Ref<SV::AsyncTexture2D> earthCloudsMap;
		static Ref<SV::AsyncTexture2D> earthNightMap;
		//
		static Ref<SV::Atmosphere::EarthAtmosphere> earthAtmosphere;
	};

	//
	class Mars : public PlanetNew
	{
	public:
		Mars(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::Texture2D> marsDiffuseMap;
		//
		static Ref<SV::Atmosphere::IAtmosphere> marsAtmosphere;
	};

	//
	class Jupiter : public PlanetNew
	{
	public:
		Jupiter(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::AsyncTexture2D> jupiterDiffuseMap;
		static Ref<SV::AsyncTexture2D> jupiterRingsMap;
		//
		static Ref<SV::Atmosphere::JupiterAtmosphere> jupiterAtmosphere;
	};

	//
	class Saturn : public PlanetNew
	{
	public:
		Saturn(const Ref<SV::VAO>& quadVAO);

	private:
		//
		static Ref<SV::AsyncTexture2D> saturnDiffuseMap;
		static Ref<SV::AsyncTexture2D> saturnRingMap;
		static Ref<SV::Atmosphere::SaturnAtmosphere> saturnAtmosphere;
		//
	};

	//
	class Titan : public PlanetNew
	{
	public:
		Titan(const Ref<SV::VAO>& quadVAO);

	private:
		static Ref<SV::Atmosphere::TitanAtmosphere> titanAtmosphere;
	};

#pragma endregion

	//
	class PlanetRingShader : public ShaderProgram
	{
	public:
		PlanetRingShader();
		//
		inline void UploadTransformationMat(const glm::mat4& value) const noexcept { this->UploadMatrixF("uTransformMatrix", value); }
		inline void UploadViewProjectionMat(const glm::mat4& value) const noexcept { this->UploadMatrixF("uViewProjMatrix", value); }
		//
		inline void UploadSunPos(const glm::vec3& pos) const noexcept { this->UploadFloat("uLightPosition", pos); }
		inline void UploadExposure(const float value) const noexcept { this->UploadFloat("uExposure", value); }
		//
		inline void UploadRingMapUnit(const TextureUnits unit) const noexcept { this->UploadInt("uRingMap", unit); }
		inline void UploadRingMap(const TextureUnits unit, const ITexture& tex) const noexcept { this->UploadTexture("uRingMap", unit, tex); }
		inline void UploadRingWidth(const float value) const noexcept { this->UploadFloat("uRingWidth", value); }
		//
		inline void UploadPlanetPos(const glm::vec3& pos) const noexcept { this->UploadFloat("uPlanetPosition", pos); }
		inline void UploadPlanetRad(const float value) const noexcept { this->UploadFloat("uPlanetRadius", value); }
		inline void UploadInvShadowSoftness(const float value) const noexcept { this->UploadFloat("uInvShadowSoftness", value); }
		//
	};

	//
	class PlanetNewRenderer final : public CelestrialRenderer<SV::PlanetNew>
	{
	public:
		//
		PlanetNewRenderer();
		//
		void Render(const glm::mat4& projectionMatrix, const SV::Camera& cam, const glm::vec3& sunPosition, const float exposure, const float frameTime);
		//
		const static glm::mat4 RING_ROTATION_CORRECTION;
		//
	private:
		//
		SV::Array<Ref<SV::PlanetNew>>& planets = CelestrialRenderer<SV::PlanetNew>::celestrialBodies;
		//
		SV::Atmosphere::AtmosphereShader surfaceProgram;
		SV::Atmosphere::AtmosphereShader skyProgram;
		SV::VAO planetVAO;
		//
		SV::VAO ringsVAO;
		SV::PlanetRingShader ringsProgram;
		//
		SV::Camera skyCamera;
	};

}