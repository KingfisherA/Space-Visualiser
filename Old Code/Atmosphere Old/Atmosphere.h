#pragma once
#pragma once
#include<vector>

#include"SV/OpenGL/OpenGL Context.h"
#include"AtmospherePrecomputer.h"
#include"SV/OpenGL/ShaderProgram.h"

typedef unsigned int TextureUnits;

namespace SV
{
	class ITexture;
	class Camera;
}

namespace SV::Atmosphere
{


	enum class Luminance
	{
		// Render the spectral radiance at kLambdaR, kLambdaG, kLambdaB.
		None,
		// Render the sRGB luminance, using an approximate (on the fly) conversion
		// from 3 spectral radiance values only
		Approximate,
		// Render the sRGB luminance, precomputed from 15 spectral radiance values
		Precomputed
	};

	enum class AtmoShaderType
	{
		SurfaceShader = 0,
		SkyShader = 1
	};

	class AtmosphereShader : public SV::ShaderProgram
	{
	public:
		AtmosphereShader(const AtmoShaderType type, const SV::String& textureHeader, const Luminance luminance);
		//
		inline void UploadTransformationMat(const glm::mat4& value) const noexcept { this->UploadMatrixF("uTransformMatrix", value); }
		inline void UploadViewProjMat(const glm::mat4& value) const noexcept { this->UploadMatrixF("uViewProjMatrix", value); }
		inline void UploadNormalMat(const glm::mat3& value) const noexcept { this->UploadMatrixF("uNormalMatrix", value); }
		inline void UploadInverseViewMat(const glm::mat4& value) const noexcept { this->UploadMatrixF("uInverseViewMatrix", value); }
		//
		inline void UploadCamPos(const glm::vec3& value) const noexcept { this->UploadFloat("uCameraPos", value); }
		inline void UploadSunPos(const glm::vec3& value) const noexcept { this->UploadFloat("uSunPosition", value); }
		inline void UploadAtmoExposure(const float value) const noexcept { this->UploadFloat("uAtmoExposure", value); }
		inline void UploadFrameTime(const float value) const noexcept { this->UploadFloat("uTime", value); }
		//
		inline void UploadDiffuseMap(const TextureUnits unit, const ITexture& tex) const noexcept { this->UploadTexture("uDiffuseMap", unit, tex); }
		inline void UploadSpecularMap(const TextureUnits unit, const ITexture* tex) const noexcept { this->UploadOptionalTexture("uSpecularMap", unit, tex); }
		inline void UploadCloudsMap(const TextureUnits unit, const ITexture* tex) const noexcept { this->UploadOptionalTexture("uCloudsMap", unit, tex); }
		inline void UploadRingShadowMap(const TextureUnits unit, const ITexture* tex) const noexcept { this->UploadOptionalTexture("uRingsShadowMap", unit, tex); }
		inline void UploadNightMap(const TextureUnits unit, const ITexture* tex) const noexcept { this->UploadOptionalTexture("uNightMap", unit, tex); }
		//
		inline void UploadMieTexture(const TextureUnits mieScatteringUnit) const
		{
			this->UploadInt("singleMieScatteringTexture", mieScatteringUnit);
		}
		inline void UploadIrradianceTetxure(const TextureUnits irradianceUnit) const
		{
			UploadInt("irradianceTexture", irradianceUnit);
		}
		inline void UploadScatteringTexture(const TextureUnits scatteringUnit) const
		{
			UploadInt("scatteringTexture", scatteringUnit);
		}
		inline void UploadTransmittanceTexture(const TextureUnits transmittanceUnit) const
		{
			UploadInt("transmittanceTexture", transmittanceUnit);
		}
		inline void UploadAtmoTexUnits(const TextureUnits transmittanceUnit, const TextureUnits scatteringUnit,
			const TextureUnits irradianceUnit, const TextureUnits mieScatteringUnit = TEX_UNIT_UNDEFINED) const
		{
			UploadInt("transmittanceTexture", transmittanceUnit);
			UploadInt("scatteringTexture", scatteringUnit);
			UploadInt("irradianceTexture", irradianceUnit);
			if (mieScatteringUnit != TEX_UNIT_UNDEFINED) UploadInt("singleMieScatteringTexture", mieScatteringUnit);
		}
		inline void UploadAtmoTextures(const TextureUnits transmittanceUnit, const SV::ITexture& transmittanceTex, const TextureUnits scatteringUnit, const SV::ITexture& scatteringTex,
			const TextureUnits irradianceUnit, const SV::ITexture& irradianceTex, const TextureUnits mieScatteringUnit, const SV::ITexture* mieScatteringTexture) const
		{
			UploadTexture("transmittanceTexture", transmittanceUnit, transmittanceTex);
			UploadTexture("scatteringTexture", scatteringUnit, scatteringTex);
			UploadTexture("irradianceTexture", irradianceUnit, irradianceTex);
			if (mieScatteringTexture != nullptr && mieScatteringUnit != TEX_UNIT_UNDEFINED) UploadTexture("singleMieScatteringTexture", mieScatteringUnit, (const SV::ITexture&) * mieScatteringTexture);
		}
		//
		void UploadWhitePoint(const glm::vec3& whiteP) const noexcept { UploadFloat("uWhitePoint", whiteP); }
		void UploadPlanetCentre(const glm::vec3& centre) const noexcept { UploadFloat("uPlanetCentre", centre); }
		void UploadSunSize(float sunAngularRadius) const noexcept { UploadFloat("uSunSize", glm::vec2(tan(sunAngularRadius), cos(sunAngularRadius))); }

	private:
		template<bool densityProfileLocations = false>
		void PopulateCache()
		{
			atmoUniformCache.AddRange({
				this->GetUniformLocation("u_AtmoParams1[0]"),
				this->GetUniformLocation("u_AtmoParams1[1]"),
				this->GetUniformLocation("u_AtmoParams1[2]"),
				this->GetUniformLocation("u_AtmoParams1[3]"),
				this->GetUniformLocation("u_AtmoParams1[4]"),
				this->GetUniformLocation("u_AtmoParams1[5]"),
				this->GetUniformLocation("u_AtmoParams2"),
				this->GetUniformLocation("u_MiePhaseFunctionG")
				});

			if constexpr (densityProfileLocations)
			{
				atmoUniformCache.AddRange({
				   this->GetUniformLocation("u_SunSpectralRadianceToLuminance"),
				   this->GetUniformLocation("u_SkySpectralRadianceToLuminance")
					});

				unsigned int i = 0;
				for (; i < MAX_DENSITY_PROFILE_LAYERS * 3; i++)
				{
					atmoUniformCache.Add(this->GetUniformLocation("u_DensityProfiles[" + SV::IntegralToString(i) + "]"));
				}
				for (i = 0; i < MAX_DENSITY_PROFILE_LAYERS * 3; i++)
				{
					atmoUniformCache.Add(this->GetUniformLocation("u_DensityProfileConstant[" + SV::IntegralToString(i) + "]"));
				}
			}

			atmoUniformCache.Trim();
		}

	public:
		template<bool uploadDensityProfile = false>
		void UploadAtmosphereParameters(const ProcessedAtmoData& atmoData)
		{
			if (atmoUniformCache.IsEmpty()) PopulateCache<uploadDensityProfile>();

			auto UploadDensityProfileLayer = [this](const unsigned int currentIndex, const DensityProfileLayer& layer) -> void
			{
				constexpr unsigned int PROFILE_LAYERS_CACHE_OFFSET = 10;
				this->UploadFloat(atmoUniformCache[(size_t)currentIndex + PROFILE_LAYERS_CACHE_OFFSET], glm::vec4(layer.width, layer.exp_term, layer.exp_scale, layer.linear_term));
				this->UploadFloat(atmoUniformCache[(size_t)currentIndex + PROFILE_LAYERS_CACHE_OFFSET + MAX_DENSITY_PROFILE_LAYERS * 3], (const float)layer.constant_term);
			};
			//
			//this->UploadFloat("u_AtmoParams1[0]", atmoData.solarIrradiance);
			//this->UploadFloat("u_AtmoParams1[1]", atmoData.rayleighScattering);
			//this->UploadFloat("u_AtmoParams1[2]", atmoData.mieScattering);
			//this->UploadFloat("u_AtmoParams1[3]", atmoData.mieExtinction);
			//this->UploadFloat("u_AtmoParams1[4]", atmoData.absorptionExtinction);
			//this->UploadFloat("u_AtmoParams1[5]", atmoData.groundAlbdeo);
			////
			//this->UploadFloat("u_AtmoParams2", glm::vec4(atmoData.sunAngularRadius, atmoData.muSMin, atmoData.bottomRadius, atmoData.topRadius));
			//this->UploadFloat("u_MiePhaseFunctionG", atmoData.miePhaseFunctionG);
			////
			//this->UploadFloat("u_SunSpectralRadianceToLuminance", atmoData.sunSpectralRadianceToLuminance);
			//this->UploadFloat("u_SkySpectralRadianceToLuminance", atmoData.skySpectralRadianceToLuminance);
			//
			this->UploadFloat(atmoUniformCache[0], atmoData.solarIrradiance);
			this->UploadFloat(atmoUniformCache[1], atmoData.rayleighScattering);
			this->UploadFloat(atmoUniformCache[2], atmoData.mieScattering);
			this->UploadFloat(atmoUniformCache[3], atmoData.mieExtinction);
			this->UploadFloat(atmoUniformCache[4], atmoData.absorptionExtinction);
			this->UploadFloat(atmoUniformCache[5], atmoData.groundAlbdeo);
			//
			this->UploadFloat(atmoUniformCache[6], glm::vec4(atmoData.sunAngularRadius, atmoData.muSMin, atmoData.bottomRadius, atmoData.topRadius));
			this->UploadFloat(atmoUniformCache[7], atmoData.miePhaseFunctionG);
			//

			if constexpr (uploadDensityProfile)
			{
				this->UploadFloat(atmoUniformCache[8], atmoData.sunSpectralRadianceToLuminance);
				this->UploadFloat(atmoUniformCache[9], atmoData.skySpectralRadianceToLuminance);
				//
				for (unsigned int i = 0; i < MAX_DENSITY_PROFILE_LAYERS; i++)
				{
					UploadDensityProfileLayer(i, atmoData.rayleighDensity[i]);
					UploadDensityProfileLayer(i + MAX_DENSITY_PROFILE_LAYERS, atmoData.mieDensity[i]);
					UploadDensityProfileLayer(i + MAX_DENSITY_PROFILE_LAYERS * 2, atmoData.absorptionDensity[i]);
				}
			}
		}
		//
		inline const AtmoShaderType GetShaderType() const noexcept { return shaderType; }
		//
	private:

		AtmoShaderType shaderType;
		SV::Array<Uniform> atmoUniformCache;
	};

	class IAtmosphere
	{
	public:
		//
		const Ref<AtmospherePrecomputer>& GetPrecomputer() const { return atmoPrecomputer; }
		const ProcessedAtmoData& GetAtmosphericData() const { SV_ASSERT_T(processedAtmoData == nullptr, SV::Atmosphere::IAtmosphere, "Cannot dereference a null pointer.");  return *processedAtmoData; }
		//
	protected:
		Ref<AtmospherePrecomputer> atmoPrecomputer = nullptr;
		const ProcessedAtmoData* processedAtmoData = nullptr;
	};

	class EarthAtmosphere : public IAtmosphere
	{
	public:
		EarthAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum = Luminance::None);
	};

	class JupiterAtmosphere : public IAtmosphere
	{
	public:
		JupiterAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum = Luminance::None);
	};

	class SaturnAtmosphere : public IAtmosphere
	{
	public:
		SaturnAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum = Luminance::None);
	};

	class TitanAtmosphere : public IAtmosphere
	{
	public:
		TitanAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum = Luminance::None);
	};

	//class AtmosphereRenderer
	//{
	//public:

	//	AtmosphereRenderer();
	//	size_t AddAtmosphere(const Ref<IAtmosphere> atmo);
	//	void Render(const glm::mat4& projectionMatrix, const Camera& cam, const float time, const float exposure, const glm::vec3& sunPosition, const SV::ITexture& earthTex,
	//		const SV::ITexture& specularMap, const SV::ITexture* cloudsTex, const SV::ITexture* ringsMap, const SV::ITexture* nightMap);

	//	static const glm::mat4 RING_ROTATION_CORRECTION;
	//	static const glm::mat4 SKY_SCALE_MAT;

	//private:
	//	SV::Array<Ref<IAtmosphere>> atmospheres;
	//	static SV::Camera skyCam;

	//	Ref<SV::VAO> ringsVAO;
	//	Ref<SV::ShaderProgram> ringsProgram;

	//	Ref<SV::VAO> sphereVAO;
	//};
};