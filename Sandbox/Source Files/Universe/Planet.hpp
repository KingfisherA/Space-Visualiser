#pragma once
#include"SV/Utilities/GlobalItems.hpp"
#include"SV/OpenGL/ShaderProgram.hpp"
#include "SV/Utilities/Maths.hpp"
#include "SV/Scene/Transformation.hpp"
#include "SV/Utilities/Array.hpp"
#include<vector>
#include "SV/Utilities/SVMemory.hpp"

typedef unsigned int TextureUnits;

namespace SV
{
	//Forward Declarations
	class Texture2D;
	class VAO;
	class ArrayBuffer;
	class Camera;
	class SphereShadowShader;

	//
	class IPlanetShader abstract : public ShaderProgram
	{
	protected:
		IPlanetShader(const VertexShader& vShader, const FragmentShader& fShader);
	public:
		//
		virtual void BindViewMat(const glm::mat4& v) { UploadMatrixF(viewMatUni, v); }
		inline void BindProjectionMat(const glm::mat4& v) { UploadMatrixF(projMatUni, v); }
		inline void BindTransformationMat(const glm::mat4& v) { UploadMatrixF(transformMatUni, v); }
		inline void BindNormalMat(const glm::mat3& v) { UploadMatrixF(normalMatUni, v); }
		//
		inline void BindRingsMap(const TextureUnits unit) { UploadInt(ringsMapUni, (int)unit); }
		inline void BindRingParameters(const glm::vec4& params) { UploadFloat(ringsParamUni, params); }
		//
		inline void BindLightPos(const glm::vec3& lightPos) { UploadFloat(lightPosUni, lightPos); }
		inline void BindLightCol(const glm::vec3& lightCol) { UploadFloat(lightColUni, lightCol); }
		//
		inline void BindSurfaceMapUnit(const TextureUnits unit) { UploadInt(surfaceMapUni, (const int)unit); }
		//
		const static glm::vec4 DefaultRingParams;
		//
	protected:
		//
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		Uniform transformMatUni = 0;
		Uniform normalMatUni = 0;
		//
		Uniform lightPosUni = 0;
		Uniform lightColUni = 0;
		//
		Uniform surfaceMapUni = 0;
		//
		Uniform ringsMapUni = 0;
		Uniform ringsParamUni = 0;
	};
	class TerrestrialShader : public IPlanetShader
	{
	public:
		TerrestrialShader();
		//
		virtual void BindViewMat(const glm::mat4& v) override
		{
			IPlanetShader::BindViewMat(v);
			UploadMatrixF(invViewMatUni, glm::inverse(v));
		}
		//
		inline void BindReflectivity(const float r) { UploadFloat(reflectivityUni, r); }
		inline void BindShineDamper(const float damper) { UploadFloat(damperUni, damper); }
		//
		inline void BindCloudsMapUnit(const TextureUnits unit) { UploadInt(cloudsMapUni, (const int)unit); }
		inline void BindSpecularMapUnit(const TextureUnits unit) { UploadInt(specularMapUni, (const int)unit); }
		inline void BindNightMapUnit(const TextureUnits unit) { UploadInt(nightMapUni, (const int)unit); }
		//
		inline void BindTime(float time) { UploadFloat(timeUni, time); }
		//
	protected:
		//
		Uniform invViewMatUni = 0;
		Uniform reflectivityUni = 0;
		Uniform damperUni = 0;
		//
		Uniform cloudsMapUni = 0;
		Uniform specularMapUni = 0;
		Uniform nightMapUni = 0;
		//
		Uniform timeUni = 0;
		//
	};
	class PlanetRingsShader : public virtual IPlanetShader
	{
	public:
		PlanetRingsShader();
		//
	};
	class GasPlanetShader : public virtual IPlanetShader
	{
	public:
		//
		GasPlanetShader();
		//
	protected:
		//
	};

#pragma region Atmosphere Classes
	/// <summary>
	/// A structure for holding atmospheric scattering data
	/// </summary>
	struct AtmosphericData
	{
	public:
		/// <summary>
		/// Create a new atmospheric data structure
		/// </summary>
		/// <param name="KrIn">Rayleigh scattering constant (how much scattering should be rayleigh scattering)</param>
		/// <param name="KmIn">Mie scattering constant (how much scattering should be mie scattering)</param>
		/// <param name="ESunIn">The sun's brightness that should be used</param>
		/// <param name="gIn">
		///The phase functions scattering constant which affects the scattering in the phase function.
		///Positive "g" values scatter more light back to the light source and negative "g" values scatter more light to the camera.
		///Note! Do not set "g" to 1 or -1 as the phase function will reduce to zero.
		/// </param>
		/// <param name="wavelengthRIn">The red wavelength of light to be scattered using this formula 1/pow(R, 4)</param>
		/// <param name="wavelengthGIn">The green wavelength of light to be scattered using this formula 1/pow(G, 4)</param>
		/// <param name="wavelengthBIn">The blue wavelength of light to be scattered using this formula 1/pow(B, 4)</param>
		AtmosphericData(float KrIn, float KmIn, float ESunIn,
			float gIn, float wavelengthRIn, float wavelengthGIn,
			float wavelengthBIn, unsigned int SamplesIn)
		{
			Kr = KrIn;
			Km = KmIn;
			ESun = ESunIn;
			g = gIn;
			WavelengthR = wavelengthRIn;
			WavelengthG = wavelengthGIn;
			WavelengthB = wavelengthBIn;
			Samples = SamplesIn;
		}
		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="data">The instance to copy into this one</param>
		AtmosphericData(const AtmosphericData& data)
		{
			Kr = data.Kr;
			Km = data.Km;
			ESun = data.ESun;
			g = data.g;
			WavelengthR = data.WavelengthR;
			WavelengthG = data.WavelengthG;
			WavelengthB = data.WavelengthB;
			Samples = data.Samples;
		}
		/// <summary>
		/// Copy an atmospheric data structure into this instance
		/// </summary>
		/// <param name="data">The data to copy from</param>
		void operator=(const AtmosphericData& data)
		{
			Kr = data.Kr;
			Km = data.Km;
			ESun = data.ESun;
			g = data.g;
			WavelengthR = data.WavelengthR;
			WavelengthG = data.WavelengthG;
			WavelengthB = data.WavelengthB;
			Samples = data.Samples;
		}
		//
		//Default values
		static const AtmosphericData DefaultValues;
		static const AtmosphericData DefaultValuesCalculated;
		static const float DefaultAdditionFactor;
		//
		//Rayleigh scattering constant (how much scattering should be
		//rayleigh scattering)
		float Kr = 0.0f;
		//Mie scattering constant (how much scattering should be
		//mie scattering)
		float Km = 0.0f;
		//The sun's brightness that should be used
		float ESun = 0.0f;
		//The phase functions scattering constant
		//Affects the scattering in the phase function
		//Positive "g" values scatter more light back to the light source
		//and negative "g" values scatter more light to the camera
		float g = 0.0f;
		//The red wavelength of light to be scattered using this formula 1/pow(R, 4)
		float WavelengthR = 0.0f;
		//The green wavelength of light to be scattered using this formula 1/pow(G, 4)
		float WavelengthG = 0.0f;
		//The blue wavelength of light to be scattered using this formula 1/pow(B, 4)
		float WavelengthB = 0.0f;
		//The number of samples taken for each ray cast
		unsigned int Samples = 1;
	};

	//
	class AtmosphereShader : public ShaderProgram
	{
	public:
		AtmosphereShader();
		//
		inline void BindMatricies(const glm::mat4& viewMat, const glm::mat4& projMat, const glm::mat4& transformMat)
		{
			UploadMatrixF(viewMatUni, viewMat);
			UploadMatrixF(projMatUni, projMat);
			UploadMatrixF(transformMatUni, transformMat);
		}
		//
		void BindAtmosphericData(const AtmosphericData& data);
		//
		void BindRadii(float innerRadius, float outerRadius, float scaleDepth = 0.25f);
		//
		inline void BindLightPos(const glm::vec3& pos) { UploadFloat(lightPosUni, pos); }
		inline void BindCamPos(const glm::vec3& pos) { UploadFloat(camPosUni, pos); }
		inline void BindTranslation(const glm::vec3& loc) { UploadFloat(translationUni, loc); }
		//
	protected:
		//
	private:
		//
		Uniform transformMatUni = 0;
		Uniform viewMatUni = 0;
		Uniform projMatUni = 0;
		//
		Uniform camPosUni = 0;
		Uniform lightPosUni = 0;
		Uniform translationUni = 0;
		//
		Uniform invWavelengthUni = 0;
		Uniform krESunUni = 0;
		Uniform kmESunUni = 0;
		Uniform kr4PiUni = 0;
		Uniform km4PiUni = 0;
		Uniform gUni = 0;
		Uniform g2Uni = 0;
		Uniform samplesUni = 0;
		//
		Uniform outerRadUni = 0;
		Uniform outerRadUni2 = 0;
		Uniform innerRadUni = 0;
		Uniform scaleUni = 0;
		Uniform scaleDepthUni = 0;
		Uniform scaleOverSDUni = 0;
	};
#pragma endregion

	//
	class IPlanet abstract : public Transformation
	{
	public:
		//
		IPlanet(Ref<Texture2D> diffuseMap) :diffuseMapTexture(diffuseMap), ringParameters(0.0f) {}
		IPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap) : diffuseMapTexture(diffuseMap), ringTexture(ringMap), ringParameters(0.0f)
		{
			if (ringMap != nullptr) ringParameters = IPlanetShader::DefaultRingParams;
		}
		IPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap, const glm::vec4& ringParam) : diffuseMapTexture(diffuseMap), ringTexture(ringMap), ringParameters(ringParam) {}
		virtual ~IPlanet() {}
		//
		IPlanet* AddRings(const Ref<Texture2D> ringMap, const glm::vec4& ringParams = PlanetRingsShader::DefaultRingParams)
		{
			ringParameters = ringParams;
			ringTexture = (Ref<Texture2D>)ringMap;

			return this;
		}
		IPlanet* AddAtmosphere(const AtmosphericData& data = AtmosphericData::DefaultValuesCalculated)
		{
			atmosphereData = data;
			hasAtmosphere = true;

			return this;
		}
		//
		Ref<Texture2D> diffuseMapTexture = nullptr;
		//
		Ref<Texture2D> ringTexture = nullptr;
		glm::vec4 ringParameters;
		//
	protected:
		friend class PlanetRenderer;
		//
		bool hasAtmosphere = false;
		AtmosphericData atmosphereData = AtmosphericData::DefaultValuesCalculated;
		//
	};
	class TerrestrialPlanet : public IPlanet
	{
	public:
		//
		TerrestrialPlanet(Ref<Texture2D> diffuseMap) :IPlanet(diffuseMap) {}
		TerrestrialPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap, const glm::vec4& ringParam) : IPlanet(diffuseMap, ringMap, ringParam) {}
		//
		TerrestrialPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap, const glm::vec4& ringParam,
			Ref<Texture2D> cloudsMapIn, Ref<Texture2D> specularMapIn, Ref<Texture2D> nightMapIn) : IPlanet(diffuseMap, ringMap, ringParam),
			cloudsMap(cloudsMapIn), nightMap(nightMapIn), specularMap(specularMapIn) {}
		TerrestrialPlanet(Ref<Texture2D> diffuseMap,
			Ref<Texture2D> cloudsMapIn, Ref<Texture2D> specularMapIn, Ref<Texture2D> nightMapIn) : IPlanet(diffuseMap),
			cloudsMap(cloudsMapIn), nightMap(nightMapIn), specularMap(specularMapIn) {}
		//
		float pReflectivity = 0.1f;
		float pShineDamper = 5.0f;
		glm::vec3 pNightMapTint = glm::vec3(1.0f);
		//
	//protected:
	//	friend class PlanetRenderer;
		//
		Ref<Texture2D> cloudsMap = nullptr;
		Ref<Texture2D> nightMap = nullptr;
		Ref<Texture2D> specularMap = nullptr;
		//
	};
	class GasPlanet : public IPlanet
	{
	public:
		//
		GasPlanet(Ref<Texture2D> diffuseMap) :IPlanet(diffuseMap) {}
		GasPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap) : IPlanet(diffuseMap, ringMap) {}
		GasPlanet(Ref<Texture2D> diffuseMap, Ref<Texture2D> ringMap, const glm::vec4& ringParam) : IPlanet(diffuseMap, ringMap, ringParam) {}
		//
		//
	};

	//
	class PlanetRenderer
	{
	public:
		PlanetRenderer();
		//~PlanetRenderer();
		//
		void AddPlanet(Ref<IPlanet> p);
		void RemovePlanet(Ref<IPlanet> p);
		//
		void RenderPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const;
		//
		static const glm::mat4 RINGS_ROTATION;
		//
	private:
		//
		void RenderTerrestrialPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const;
		void RenderGasPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const;
		void RenderAtmospheres(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix) const;
		void RenderPlanetRings(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const;
		//
		std::vector<Ref<TerrestrialPlanet>> terrestrialPlanets;
		std::vector<Ref<GasPlanet>> gasPlanets;
		//
		Ref<PlanetRingsShader> pRingsShader = nullptr;
		Ref<TerrestrialShader> terrestrialShader = nullptr;
		Ref<AtmosphereShader> atmoShader = nullptr;
		//
		Ref<SphereShadowShader> ringsShader = nullptr;
		Ref<VAO> sphereVAO = nullptr;
		Ref<VAO> torusVAO = nullptr;
	};
}