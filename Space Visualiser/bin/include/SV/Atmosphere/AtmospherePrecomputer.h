#pragma once
#include"SV/OpenGL/OpenGL Context.h"
#include"SV/OpenGL/Texture.h"
#include<initializer_list>
#include<string>
#include<array>

namespace SV
{
	class VAO;
	class ShaderProgram;
	class FBO;
	class FragmentShader;

	struct Dimension2D;
	struct Dimension3D;
}

namespace SV::Atmosphere
{

#pragma region Constants

	constexpr int TRANSMITTANCE_TEXTURE_WIDTH = 256;
	constexpr int TRANSMITTANCE_TEXTURE_HEIGHT = 64;

	constexpr int SCATTERING_TEXTURE_R_SIZE = 32;
	constexpr int SCATTERING_TEXTURE_MU_SIZE = 128;
	constexpr int SCATTERING_TEXTURE_MU_S_SIZE = 32;
	constexpr int SCATTERING_TEXTURE_NU_SIZE = 8;

	constexpr int SCATTERING_TEXTURE_WIDTH =
		SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
	constexpr int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
	constexpr int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;

	constexpr int IRRADIANCE_TEXTURE_WIDTH = 64;
	constexpr int IRRADIANCE_TEXTURE_HEIGHT = 16;

	// The conversion factor between watts and lumens.
	constexpr double MAX_LUMINOUS_EFFICACY = 683.0;

	// Values from "CIE (1931) 2-deg color matching functions", see
	// "http://web.archive.org/web/20081228084047/
	//    http://www.cvrl.org/database/data/cmfs/ciexyz31.txt".
	constexpr double CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[380] =
	{
	  360, 0.000129900000, 0.000003917000, 0.000606100000,
	  365, 0.000232100000, 0.000006965000, 0.001086000000,
	  370, 0.000414900000, 0.000012390000, 0.001946000000,
	  375, 0.000741600000, 0.000022020000, 0.003486000000,
	  380, 0.001368000000, 0.000039000000, 0.006450001000,
	  385, 0.002236000000, 0.000064000000, 0.010549990000,
	  390, 0.004243000000, 0.000120000000, 0.020050010000,
	  395, 0.007650000000, 0.000217000000, 0.036210000000,
	  400, 0.014310000000, 0.000396000000, 0.067850010000,
	  405, 0.023190000000, 0.000640000000, 0.110200000000,
	  410, 0.043510000000, 0.001210000000, 0.207400000000,
	  415, 0.077630000000, 0.002180000000, 0.371300000000,
	  420, 0.134380000000, 0.004000000000, 0.645600000000,
	  425, 0.214770000000, 0.007300000000, 1.039050100000,
	  430, 0.283900000000, 0.011600000000, 1.385600000000,
	  435, 0.328500000000, 0.016840000000, 1.622960000000,
	  440, 0.348280000000, 0.023000000000, 1.747060000000,
	  445, 0.348060000000, 0.029800000000, 1.782600000000,
	  450, 0.336200000000, 0.038000000000, 1.772110000000,
	  455, 0.318700000000, 0.048000000000, 1.744100000000,
	  460, 0.290800000000, 0.060000000000, 1.669200000000,
	  465, 0.251100000000, 0.073900000000, 1.528100000000,
	  470, 0.195360000000, 0.090980000000, 1.287640000000,
	  475, 0.142100000000, 0.112600000000, 1.041900000000,
	  480, 0.095640000000, 0.139020000000, 0.812950100000,
	  485, 0.057950010000, 0.169300000000, 0.616200000000,
	  490, 0.032010000000, 0.208020000000, 0.465180000000,
	  495, 0.014700000000, 0.258600000000, 0.353300000000,
	  500, 0.004900000000, 0.323000000000, 0.272000000000,
	  505, 0.002400000000, 0.407300000000, 0.212300000000,
	  510, 0.009300000000, 0.503000000000, 0.158200000000,
	  515, 0.029100000000, 0.608200000000, 0.111700000000,
	  520, 0.063270000000, 0.710000000000, 0.078249990000,
	  525, 0.109600000000, 0.793200000000, 0.057250010000,
	  530, 0.165500000000, 0.862000000000, 0.042160000000,
	  535, 0.225749900000, 0.914850100000, 0.029840000000,
	  540, 0.290400000000, 0.954000000000, 0.020300000000,
	  545, 0.359700000000, 0.980300000000, 0.013400000000,
	  550, 0.433449900000, 0.994950100000, 0.008749999000,
	  555, 0.512050100000, 1.000000000000, 0.005749999000,
	  560, 0.594500000000, 0.995000000000, 0.003900000000,
	  565, 0.678400000000, 0.978600000000, 0.002749999000,
	  570, 0.762100000000, 0.952000000000, 0.002100000000,
	  575, 0.842500000000, 0.915400000000, 0.001800000000,
	  580, 0.916300000000, 0.870000000000, 0.001650001000,
	  585, 0.978600000000, 0.816300000000, 0.001400000000,
	  590, 1.026300000000, 0.757000000000, 0.001100000000,
	  595, 1.056700000000, 0.694900000000, 0.001000000000,
	  600, 1.062200000000, 0.631000000000, 0.000800000000,
	  605, 1.045600000000, 0.566800000000, 0.000600000000,
	  610, 1.002600000000, 0.503000000000, 0.000340000000,
	  615, 0.938400000000, 0.441200000000, 0.000240000000,
	  620, 0.854449900000, 0.381000000000, 0.000190000000,
	  625, 0.751400000000, 0.321000000000, 0.000100000000,
	  630, 0.642400000000, 0.265000000000, 0.000049999990,
	  635, 0.541900000000, 0.217000000000, 0.000030000000,
	  640, 0.447900000000, 0.175000000000, 0.000020000000,
	  645, 0.360800000000, 0.138200000000, 0.000010000000,
	  650, 0.283500000000, 0.107000000000, 0.000000000000,
	  655, 0.218700000000, 0.081600000000, 0.000000000000,
	  660, 0.164900000000, 0.061000000000, 0.000000000000,
	  665, 0.121200000000, 0.044580000000, 0.000000000000,
	  670, 0.087400000000, 0.032000000000, 0.000000000000,
	  675, 0.063600000000, 0.023200000000, 0.000000000000,
	  680, 0.046770000000, 0.017000000000, 0.000000000000,
	  685, 0.032900000000, 0.011920000000, 0.000000000000,
	  690, 0.022700000000, 0.008210000000, 0.000000000000,
	  695, 0.015840000000, 0.005723000000, 0.000000000000,
	  700, 0.011359160000, 0.004102000000, 0.000000000000,
	  705, 0.008110916000, 0.002929000000, 0.000000000000,
	  710, 0.005790346000, 0.002091000000, 0.000000000000,
	  715, 0.004109457000, 0.001484000000, 0.000000000000,
	  720, 0.002899327000, 0.001047000000, 0.000000000000,
	  725, 0.002049190000, 0.000740000000, 0.000000000000,
	  730, 0.001439971000, 0.000520000000, 0.000000000000,
	  735, 0.000999949300, 0.000361100000, 0.000000000000,
	  740, 0.000690078600, 0.000249200000, 0.000000000000,
	  745, 0.000476021300, 0.000171900000, 0.000000000000,
	  750, 0.000332301100, 0.000120000000, 0.000000000000,
	  755, 0.000234826100, 0.000084800000, 0.000000000000,
	  760, 0.000166150500, 0.000060000000, 0.000000000000,
	  765, 0.000117413000, 0.000042400000, 0.000000000000,
	  770, 0.000083075270, 0.000030000000, 0.000000000000,
	  775, 0.000058706520, 0.000021200000, 0.000000000000,
	  780, 0.000041509940, 0.000014990000, 0.000000000000,
	  785, 0.000029353260, 0.000010600000, 0.000000000000,
	  790, 0.000020673830, 0.000007465700, 0.000000000000,
	  795, 0.000014559770, 0.000005257800, 0.000000000000,
	  800, 0.000010253980, 0.000003702900, 0.000000000000,
	  805, 0.000007221456, 0.000002607800, 0.000000000000,
	  810, 0.000005085868, 0.000001836600, 0.000000000000,
	  815, 0.000003581652, 0.000001293400, 0.000000000000,
	  820, 0.000002522525, 0.000000910930, 0.000000000000,
	  825, 0.000001776509, 0.000000641530, 0.000000000000,
	  830, 0.000001251141, 0.000000451810, 0.000000000000,
	};

	// The conversion matrix from XYZ to linear sRGB color spaces.
	// Values from https://en.wikipedia.org/wiki/SRGB.
	constexpr double XYZ_TO_SRGB[9] =
	{
	  +3.2406, -1.5372, -0.4986,
	  -0.9689, +1.8758, +0.0415,
	  +0.0557, -0.2040, +1.0570
	};

	constexpr glm::vec3 K_LAMBDA = { 680.0f, 550.0f, 440.0f };
	constexpr unsigned int MAX_DENSITY_PROFILE_LAYERS = 2;

	// Whether to pack the (red component of the) sin gle Mie scattering with the
// Rayleigh and multiple scattering in a sin gle texture, or to store the
// (3 components of the) sin gle Mie scattering in a separate texture.
	constexpr bool COMBINE_SCATTERING_TEXTURES = true;

	constexpr double kPi = 3.1415926;
	constexpr double SUN_ANGULAR_RADIUS = 0.00935 / 2.0;
	constexpr double kSunSolidAngle = kPi * SUN_ANGULAR_RADIUS * SUN_ANGULAR_RADIUS;
	constexpr double kLengthUnitInMeters = 1000.0;

#pragma endregion

	// An atmosphere layer of width 'width' (in m), and whose density is defined as
	//   'exp_term' * exp('exp_scale' * h) + 'linear_term' * h + 'constant_term',
	// clamped to [0,1], and where h is the altitude (in m). 'exp_term' and
	// 'constant_term' are unitless, while 'exp_scale' and 'linear_term' are in
	// m^-1.
	class DensityProfileLayer
	{
	public:
		DensityProfileLayer() : DensityProfileLayer(0.0, 0.0, 0.0, 0.0, 0.0) {}
		DensityProfileLayer(double width, double exp_term, double exp_scale,
			double linear_term, double constant_term)
			: width(width), exp_term(exp_term), exp_scale(exp_scale),
			linear_term(linear_term), constant_term(constant_term) {
		}
		//
		void operator=(const DensityProfileLayer& layer)
		{
			width = layer.width;
			exp_term = layer.exp_term;
			exp_scale = layer.exp_scale;
			linear_term = layer.linear_term;
			constant_term = layer.constant_term;
		}
		//
		double width;
		double exp_term;
		double exp_scale;
		double linear_term;
		double constant_term;
	};

	//A structure to hold the raw atmospheric properties, that have undergone no interpolation or steralisation for shader upload
	class WavelengthData
	{
	public:
		//
#pragma region Initilisation
		WavelengthData(const std::vector<double>& wavelengthsIn, const std::vector<double>& solarIrradianceIn, const double sunAngularRadiusIn, const double bottomRadiusIn, const double topRadiusIn,
			const std::vector<DensityProfileLayer>& rayleighDensityIn, const std::vector<double>& rayleighScatteringIn, const std::vector<DensityProfileLayer>& mieDensityIn, const std::vector<double>& mieScatteringIn,
			const std::vector<double>& mieExtinctionIn, const double miePhaseFunctionGIn, const std::vector<DensityProfileLayer>& absorptionDensityIn, const std::vector<double>& absorptionExtinctionIn,
			const std::vector<double>& groundAlbedoIn, const double maxSunZenithAngleIn) :
			wavelengths(wavelengthsIn),
			solarIrradiance(solarIrradianceIn),
			sunAngularRadius(sunAngularRadiusIn),
			bottomRadius(bottomRadiusIn),
			topRadius(topRadiusIn),
			rayleighDensity(rayleighDensityIn),
			rayleighScattering(rayleighScatteringIn),
			mieDensity(mieDensityIn),
			mieScattering(mieScatteringIn),
			mieExtinction(mieExtinctionIn),
			miePhaseFunctionG(miePhaseFunctionGIn),
			absorptionDensity(absorptionDensityIn),
			absorptionExtinction(absorptionExtinctionIn),
			groundAlbedo(groundAlbedoIn),
			maxSunZenithAngle(maxSunZenithAngleIn) {}
		// The wavelength values, in nanometers, and sorted in increasing order, for
// which the solar_irradiance, rayleigh_scattering, mie_scattering,
// mie_extinction and ground_albedo samples are provided. If your shaders
// use luminance values (as opposed to radiance values, see above), use a
// large number of wavelengths (e.g. between 15 and 50) to get accurate
// results (this number of wavelengths has absolutely no impact on the
// shader performance).
		const std::vector<double> wavelengths;
		// The solar irradiance at the top of the atmosphere, in W/m^2/nm. This
// vector must have the same size as the wavelengths parameter.
		const std::vector<double> solarIrradiance;
		// The sun's angular radius, in radians. Warning: the implementation uses
// approximations that are valid only if this value is smaller than 0.1.
		double sunAngularRadius;
		// The distance between the planet center and the bottom of the atmosphere,
// in m.
		double bottomRadius;
		// The distance between the planet center and the top of the atmosphere,
		// in m.
		double topRadius;
		// The density profile of air molecules, i.e.a function from altitude to
			// dimensionless values between 0 (nullptr density) and 1 (maximum density).
			// Layers must be sorted from bottom to top. The width of the last layer is
			// ignored, i.e. it always extend to the top atmosphere boundary. At most 2
			// layers can be specified.
		const std::vector<DensityProfileLayer> rayleighDensity;
		// The scattering coefficient of air molecules at the altitude where their
			// density is maximum (usually the bottom of the atmosphere), as a function
			// of wavelength, in m^-1. The scattering coefficient at altitude h is equal
			// to 'rayleigh_scattering' times 'rayleigh_density' at this altitude. This
			// vector must have the same size as the wavelengths parameter.
		const std::vector<double>  rayleighScattering;
		// The density profile of aerosols, i.e. a function from altitude to
		// dimensionless values between 0 (nullptr density) and 1 (maximum density).
		// Layers must be sorted from bottom to top. The width of the last layer is
		// ignored, i.e. it always extend to the top atmosphere boundary. At most 2
		// layers can be specified.
		const std::vector<DensityProfileLayer> mieDensity;
		// The scattering coefficient of aerosols at the altitude where their
		// density is maximum (usually the bottom of the atmosphere), as a function
		// of wavelength, in m^-1. The scattering coefficient at altitude h is equal
		// to 'mie_scattering' times 'mie_density' at this altitude. This vector
		// must have the same size as the wavelengths parameter.
		const std::vector<double> mieScattering;
		// The extinction coefficient of aerosols at the altitude where their
		// density is maximum (usually the bottom of the atmosphere), as a function
		// of wavelength, in m^-1. The extinction coefficient at altitude h is equal
		// to 'mie_extinction' times 'mie_density' at this altitude. This vector
		// must have the same size as the wavelengths parameter.
		const std::vector<double> mieExtinction;
		// The asymetry parameter for the Cornette-Shanks phase function for the
		// aerosols.
		double miePhaseFunctionG;
		// The density profile of air molecules that absorb light (e.g. ozone), i.e.
		// a function from altitude to dimensionless values between 0 (nullptr density)
		// and 1 (maximum density). Layers must be sorted from bottom to top. The
		// width of the last layer is ignored, i.e. it always extend to the top
		// atmosphere boundary. At most 2 layers can be specified.
		const std::vector<DensityProfileLayer> absorptionDensity;
		// The extinction coefficient of molecules that absorb light (e.g. ozone) at
		// the altitude where their density is maximum, as a function of wavelength,
		// in m^-1. The extinction coefficient at altitude h is equal to
		// 'absorption_extinction' times 'absorption_density' at this altitude. This
		// vector must have the same size as the wavelengths parameter.
		const std::vector<double> absorptionExtinction;
		// The average albedo of the ground, as a function of wavelength. This
		// vector must have the same size as the wavelengths parameter.
		const std::vector<double> groundAlbedo;
		// The maximum Sun zenith an gle for which atmospheric scattering must be
		// precomputed, in radians (for maximum precision, use the smallest Sun
		// zenith an gle yielding ne gligible sky light radiance values. For instance,
		// for the Earth case, 102 degrees is a good choice for most cases (120
		// degrees is necessary for very high exposure values).
		double maxSunZenithAngle;
#pragma endregion
		//
	};

	struct ScatteringData
	{
		//static constexpr unsigned int noOfValues = (kLambdaMax - kLambdaMin) / 10 + 1;
	public:

		ScatteringData() noexcept;
		ScatteringData(const ScatteringData& dataIn) noexcept;
		ScatteringData(ScatteringData&& dataIn) noexcept : ScatteringData((const ScatteringData&)dataIn) {}

		ScatteringData& operator=(const ScatteringData&) = default;

		unsigned int kLambdaMin;
		unsigned int kLambdaMax;

		SV::ArrayView<double> solarIrradiance;

		double kBottomRadius;
		double kTopRadius;

		double kRayleighScatteringCoefficient;
		SV::ArrayView<double> rayleighExtinction;
		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> rayleighDensity;

		double kMieAngstromAlpha;
		double kMieAngstromBeta;
		double kMieSingleScatteringAlbedo;
		double kMiePhaseFunctionG;
		double kMieScaleHeight;
		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> mieDensity;

		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> absorptionDensity;

		double kGroundAlbedo;
	};

	//A structure to hold the atmospheric parameters that have undergone processing through means of interpolation and steralisation.
	//The data held within this structure is ready for upload to the shaders
	struct ProcessedAtmoData
	{
	public:
		ProcessedAtmoData() noexcept;
		ProcessedAtmoData(const ProcessedAtmoData& dataIn) noexcept;
		ProcessedAtmoData(ProcessedAtmoData&& dataIn) noexcept : ProcessedAtmoData((const ProcessedAtmoData&)dataIn) {}
		ProcessedAtmoData(const glm::vec3& solarIrradianceIn,
			const float sunAngularRadiusIn,
			const float bottomRadiusIn,
			const float topRadiusIn,
			const std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>& rayleighDensityIn,
			const glm::vec3& rayleighScatteringIn,
			const std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>& mieDensityIn,
			const glm::vec3& mieScatteringIn,
			const glm::vec3& mieExtinctionIn,
			const float miePhaseFunctionGIn,
			const std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>& absorptionDensityIn,
			const glm::vec3& absorptionExtinctionIn,
			const glm::vec3& groundAlbdeoIn,
			const float muSMinIn,
			const glm::vec3& sunSpectralRadianceToLuminanceIn,
			const glm::vec3& skySpectralRadianceToLuminanceIn,
			const float surfaceAtmoIntensityIn) noexcept;

		ProcessedAtmoData& operator=(const ProcessedAtmoData&) = default;

		glm::vec3 solarIrradiance;
		float sunAngularRadius;
		float bottomRadius;
		float topRadius;

		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> rayleighDensity;
		glm::vec3 rayleighScattering;

		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> mieDensity;
		glm::vec3 mieScattering;
		glm::vec3 mieExtinction;
		float miePhaseFunctionG;

		std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS> absorptionDensity;
		glm::vec3 absorptionExtinction;

		glm::vec3 groundAlbdeo;
		float muSMin;

		glm::vec3 sunSpectralRadianceToLuminance;
		glm::vec3 skySpectralRadianceToLuminance;

		mutable float surfaceAtmoIntensity;
	};

	class AtmospherePrecomputer
	{
	public:

		AtmospherePrecomputer(
			const ScatteringData& atmoData,
			double lengthUnitInMeters,
			Ref<SV::VAO> fullScreenQuadIn,
			unsigned int numPrecomputedWavelengths = 3);
		AtmospherePrecomputer(
			const ProcessedAtmoData& atmoData,
			// The length unit used in your shaders and meshes. This is the length unit
			// which must be used when calling the atmosphere model shader functions.
			double lengthUnitInMeters,
			Ref<SV::VAO> fullScreenQuadIn,
			// The number of wavelengths for which atmospheric scattering must be
	// precomputed (the temporary GPU memory used during precomputations, and
	// the GPU memory used by the precomputed results, is independent of this
	// number, but the <i>precomputation time is directly proportional to this
	// number</i>):
	// - if this number is less than or equal to 3, scattering is precomputed
	// for 3 wavelengths, and stored as irradiance values. Then both the
	// radiance-based and the luminance-based API functions are provided (see
	// the above note).
	// - otherwise, scattering is precomputed for this number of wavelengths
	// (rounded up to a multiple of 3), integrated with the CIE color matching
	// functions, and stored as illuminance values. Then only the
	// luminance-based API functions are provided (see the above note).
			unsigned int numPrecomputedWavelengths = 3);

		~AtmospherePrecomputer() = default;

		static double CalculateRayleighCoefficient(double indexOfRefraction);

		static ProcessedAtmoData ProcessAtmosphericData(
			const ScatteringData& scatteringData,
			const double lengthUnit = kLengthUnitInMeters,
			const bool precomputeIlluminance = false,
			const bool halfPrecision = true,
			const glm::vec3& lambdas = K_LAMBDA);

		static SV::String CompileAtmosphereDataToString(const ProcessedAtmoData& data);
		static void RecompileTextureHeader();
		static SV::String CompileAtmosphereHeaderToString(const SV::Dimension2D transmittanceTexDimensions, const glm::uivec4& scatteringTexProperties,
			const SV::Dimension2D irradianceTexDimensions, const bool texturesCombined);

		void PrecomputeTextures(unsigned int numScatteringOrders = 4);

		void SetProgramUniforms(
			SV::ShaderProgram& program,
			const TextureUnits transmittance_texture_unit,
			const TextureUnits scattering_texture_unit,
			const TextureUnits irradiance_texture_unit,
			const TextureUnits optional_single_mie_scattering_texture_unit = 0) const;

		// Utility method to convert a function of the wavelength to linear sRGB.
		// 'wavelengths' and 'spectrum' must have the same size. The integral of
		// 'spectrum' times each CIE_2_DEG_COLOR_MATCHING_FUNCTIONS (and times
		// MAX_LUMINOUS_EFFICACY) is computed to get XYZ values, which are then
		// converted to linear sRGB with the XYZ_TO_SRGB matrix.
		static void ConvertSpectrumToLinearSrgb(
			const std::vector<double>& wavelengths,
			const std::vector<double>& spectrum,
			double* r, double* g, double* b);

		//Red, green and blue average wavelengths; lambda for rayleigh scattering equation
		//static constexpr double kLambdaR = 680.0;
		//static constexpr double kLambdaG = 550.0;
		//static constexpr double kLambdaB = 440.0;

		//std::function<std::string(const glm::vec3&)> GetAtmoParamHeader = nullptr;

		inline const double GetLengthUnit()const { return lengthUnitInMetres; }
		inline const unsigned int GetPrecomputedWavelengths()const { return precomputedWavelengths; }
		inline const ProcessedAtmoData& GetProcessedAtmoData()const { return processedData; }
		inline const SV::String& GetAtmosphereDataString() const { return atmosphericDataString; }
		inline static const SV::String& GetTextureHeaderString()
		{
			if (textureSizeHeader == "") RecompileTextureHeader();
			return textureSizeHeader;
		}
		inline const SV::String GetAtmosphereDataHeader()const { return textureSizeHeader + atmosphericDataString; }

		static double CieColorMatchingFunctionTableValue(double wavelength, int column);
		static double Interpolate(const std::vector<double>& wavelengths, const std::vector<double>& wavelength_function,
			double wavelength);

		static void ComputeSpectralRadianceToLuminanceFactors(const std::vector<double>& wavelengths, const std::vector<double>& solar_irradiance, double lambda_power,
			double* k_r, double* k_g, double* k_b);

		inline const Ref<SV::Texture2D> GetTransmittanceTex() const { return transmittanceTexture; }
		inline const Ref<SV::Texture3D> GetScatteringTex() const { return scatteringTexture; }
		inline const Ref<SV::Texture3D> GetMieScatteringTex() const { return singleMieScatteringTexture; }
		inline const Ref<SV::Texture2D> GetIrradianceTex() const { return irradianceTexture; }

	private:
		ProcessedAtmoData processedData;
		//WavelengthData rawData;
		double lengthUnitInMetres = 1000;
		unsigned int precomputedWavelengths = 3;

		static SV::String textureSizeHeader;
		SV::String atmosphericDataString;

		void DrawQuad(const std::initializer_list<bool>& enable_blend);

		void Precompute(
			SV::FBO& fbo,
			SV::Texture2D& deltaIrradianceTexture,
			SV::Texture3D& deltaRayleighScatteringTexture,
			SV::Texture3D& deltaMieScatteringTexture,
			SV::Texture3D& deltaScatteringDensityTexture,
			SV::Texture3D& deltaMultipleScatteringTexture,
			const glm::vec3& lambdas,
			const  glm::mat3& luminance_from_radiance,
			bool blend,
			unsigned int numScatteringOrders);

		Ref<SV::Texture2D> transmittanceTexture = nullptr;
		Ref<SV::Texture3D> scatteringTexture = nullptr;
		Ref<SV::Texture3D> singleMieScatteringTexture = nullptr;
		Ref<SV::Texture2D> irradianceTexture = nullptr;
		Ref<SV::VAO> fullScreenQuad = nullptr;
	};

}
