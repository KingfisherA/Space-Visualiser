#include "AtmospherePrecomputer.hpp"

#include <cassert>
#include <cmath>
#include <string>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Platform/GPUContext.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"

namespace SV::Atmosphere
{
	//
	ScatteringData::ScatteringData() noexcept :
		kLambdaMin(0),
		kLambdaMax(0),
		solarIrradiance(),
		kBottomRadius(0),
		kTopRadius(0),
		kRayleighScatteringCoefficient(0),
		rayleighExtinction(),
		rayleighDensity(),
		kMieAngstromAlpha(0),
		kMieAngstromBeta(0),
		kMieSingleScatteringAlbedo(0),
		kMiePhaseFunctionG(0),
		kMieScaleHeight(0),
		mieDensity(),
		absorptionDensity(),
		kGroundAlbedo(0)
	{
	}
	ScatteringData::ScatteringData(const ScatteringData& dataIn) noexcept :
		kLambdaMin(dataIn.kLambdaMin),
		kLambdaMax(dataIn.kLambdaMax),
		solarIrradiance(dataIn.solarIrradiance),
		kBottomRadius(dataIn.kBottomRadius),
		kTopRadius(dataIn.kTopRadius),
		kRayleighScatteringCoefficient(dataIn.kRayleighScatteringCoefficient),
		rayleighExtinction(dataIn.rayleighExtinction),
		rayleighDensity(dataIn.rayleighDensity),
		kMieAngstromAlpha(dataIn.kMieAngstromAlpha),
		kMieAngstromBeta(dataIn.kMieAngstromBeta),
		kMieSingleScatteringAlbedo(dataIn.kMieSingleScatteringAlbedo),
		kMiePhaseFunctionG(dataIn.kMiePhaseFunctionG),
		kMieScaleHeight(dataIn.kMieScaleHeight),
		mieDensity(dataIn.mieDensity),
		absorptionDensity(dataIn.absorptionDensity),
		kGroundAlbedo(dataIn.kGroundAlbedo)
	{
	}
	//
	ProcessedAtmoData::ProcessedAtmoData() noexcept :
		solarIrradiance(0),
		sunAngularRadius(0),
		bottomRadius(0),
		topRadius(0),
		rayleighDensity(),
		rayleighScattering(0),
		mieScattering(0),
		mieExtinction(0),
		miePhaseFunctionG(0),
		absorptionDensity(),
		absorptionExtinction(0),
		groundAlbdeo(0),
		muSMin(0),
		sunSpectralRadianceToLuminance(0),
		skySpectralRadianceToLuminance(0),
		surfaceAtmoIntensity(0)
	{
	}
	ProcessedAtmoData::ProcessedAtmoData(const ProcessedAtmoData& dataIn) noexcept :
		solarIrradiance(dataIn.solarIrradiance),
		sunAngularRadius(dataIn.sunAngularRadius),
		bottomRadius(dataIn.bottomRadius),
		topRadius(dataIn.topRadius),
		rayleighDensity(dataIn.rayleighDensity),
		rayleighScattering(dataIn.rayleighScattering),
		mieScattering(dataIn.mieScattering),
		mieExtinction(dataIn.mieExtinction),
		miePhaseFunctionG(dataIn.miePhaseFunctionG),
		absorptionDensity(dataIn.absorptionDensity),
		absorptionExtinction(dataIn.absorptionExtinction),
		groundAlbdeo(dataIn.groundAlbdeo),
		muSMin(dataIn.muSMin),
		sunSpectralRadianceToLuminance(dataIn.sunSpectralRadianceToLuminance),
		skySpectralRadianceToLuminance(dataIn.skySpectralRadianceToLuminance),
		surfaceAtmoIntensity(dataIn.surfaceAtmoIntensity)
	{
	}
	ProcessedAtmoData::ProcessedAtmoData(const glm::vec3& solarIrradianceIn,
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
		const float surfaceAtmoIntensityIn) noexcept :
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
		groundAlbdeo(groundAlbdeoIn),
		muSMin(muSMinIn),
		sunSpectralRadianceToLuminance(sunSpectralRadianceToLuminanceIn),
		skySpectralRadianceToLuminance(skySpectralRadianceToLuminanceIn),
		surfaceAtmoIntensity(surfaceAtmoIntensityIn)
	{
	}
	//

	constexpr unsigned int kLambdaMin = 360;
	constexpr unsigned int kLambdaMax = 830;

	double AtmospherePrecomputer::CieColorMatchingFunctionTableValue(double wavelength, int column)
	{
		if (wavelength <= kLambdaMin || wavelength >= kLambdaMax) {
			return 0.0;
		}
		double u = (wavelength - kLambdaMin) / 5.0;
		int row = static_cast<int>(std::floor(u));
		assert(row >= 0 && row + 1 < 95);
		assert(CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row] <= wavelength &&
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1)] >= wavelength);
		u -= row;
		return CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row + column] * (1.0 - u) +
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1) + column] * u;
	}

	double AtmospherePrecomputer::Interpolate(const std::vector<double>& wavelengths, const std::vector<double>& wavelength_function, double wavelength)
	{
		assert(wavelength_function.size() == wavelengths.size());
		if (wavelength < wavelengths[0]) {
			return wavelength_function[0];
		}
		for (unsigned int i = 0; i < wavelengths.size() - 1; ++i) {
			if (wavelength < wavelengths[i + 1]) {
				double u = (wavelength - wavelengths[(size_t)i]) / (wavelengths[(size_t)i + 1] - wavelengths[(size_t)i]);
				double p = wavelength_function[(size_t)i] * (1.0 - u) + wavelength_function[(size_t)i + 1] * u;
				return p;
			}
		}
		return wavelength_function[wavelength_function.size() - 1];
	}

	// The returned constants are in lumen.nm / watt.
	void AtmospherePrecomputer::ComputeSpectralRadianceToLuminanceFactors(
		const std::vector<double>& wavelengths,
		const std::vector<double>& solar_irradiance,
		double lambda_power, double* k_r, double* k_g, double* k_b) {
		*k_r = 0.0;
		*k_g = 0.0;
		*k_b = 0.0;
		double solar_r = Interpolate(wavelengths, solar_irradiance, K_LAMBDA.r);
		double solar_g = Interpolate(wavelengths, solar_irradiance, K_LAMBDA.g);
		double solar_b = Interpolate(wavelengths, solar_irradiance, K_LAMBDA.b);
		constexpr unsigned int dlambda = 1;
		for (unsigned int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda) {
			double x_bar = CieColorMatchingFunctionTableValue(lambda, 1);
			double y_bar = CieColorMatchingFunctionTableValue(lambda, 2);
			double z_bar = CieColorMatchingFunctionTableValue(lambda, 3);
			const double* xyz2srgb = XYZ_TO_SRGB;
			double r_bar =
				xyz2srgb[0] * x_bar + xyz2srgb[1] * y_bar + xyz2srgb[2] * z_bar;
			double g_bar =
				xyz2srgb[3] * x_bar + xyz2srgb[4] * y_bar + xyz2srgb[5] * z_bar;
			double b_bar =
				xyz2srgb[6] * x_bar + xyz2srgb[7] * y_bar + xyz2srgb[8] * z_bar;
			double irradiance = Interpolate(wavelengths, solar_irradiance, lambda);
			*k_r += r_bar * irradiance / solar_r *
				pow(lambda / K_LAMBDA.r, lambda_power);
			*k_g += g_bar * irradiance / solar_g *
				pow(lambda / K_LAMBDA.g, lambda_power);
			*k_b += b_bar * irradiance / solar_b *
				pow(lambda / K_LAMBDA.b, lambda_power);
		}
		*k_r *= MAX_LUMINOUS_EFFICACY * dlambda;
		*k_g *= MAX_LUMINOUS_EFFICACY * dlambda;
		*k_b *= MAX_LUMINOUS_EFFICACY * dlambda;
	}

	double AtmospherePrecomputer::CalculateRayleighCoefficient(double indexOfRefraction)
	{
		double kn2T = (indexOfRefraction * indexOfRefraction) - 1.0; // (n^2 -1)
		return (0.5 * (kPi * kPi) * kn2T * kn2T); //Rayleigh Scattering Coefficient, without wavelength term and molecular constant (original value: 1.24062e-6)
	}

	ProcessedAtmoData AtmospherePrecomputer::ProcessAtmosphericData(
		const ScatteringData& scatteringData,
		const double lengthUnit,
		const bool precomputeIlluminance,
		const bool halfPrecision,
		const glm::vec3& lambdas)
	{
		auto& scatSolarIrr = scatteringData.solarIrradiance;
		auto& scatRayExtinct = scatteringData.rayleighExtinction;

		std::vector<double> wavelengths;
		std::vector<double> solarIrradiance;
		std::vector<double> rayleighScattering;
		std::vector<double> mieScattering;
		std::vector<double> mieExtinction;
		std::vector<double> absorptionExtinction;
		std::vector<double> groundAlbedo;
		for (unsigned int j = scatteringData.kLambdaMin; j <= scatteringData.kLambdaMax; j += 10)
		{
			unsigned int index = (j - scatteringData.kLambdaMin) / 10;
			double lambdaWavelength = static_cast<double>(j) * 1e-3;  // micro-meters
			double lambda4 = pow(lambdaWavelength, -4);
			double mie = scatteringData.kMieAngstromBeta / scatteringData.kMieScaleHeight * pow(lambdaWavelength, -scatteringData.kMieAngstromAlpha);
			wavelengths.push_back(j);
			solarIrradiance.push_back(scatSolarIrr[size_t(index) + 1 > scatSolarIrr.size() ? scatSolarIrr.size() - 1 : index]);
			rayleighScattering.push_back(scatteringData.kRayleighScatteringCoefficient * lambda4);
			mieScattering.push_back(mie * scatteringData.kMieSingleScatteringAlbedo);
			mieExtinction.push_back(mie);

			//	const double rayleighExtinction = ;//+ ozoneExtinction;
			absorptionExtinction.push_back(scatRayExtinct[size_t(index) + 1 > scatRayExtinct.size() ? scatRayExtinct.size() - 1 : index] * lambda4); //Rayleigh Extinction
			groundAlbedo.push_back(scatteringData.kGroundAlbedo * lambda4);
		}

		auto InterpolateScatteringSpectrum = [&lambdas, &wavelengths](const std::vector<double>& scatteringWavelength, double scale) -> glm::vec3
		{
			return glm::vec3(AtmospherePrecomputer::Interpolate(wavelengths, scatteringWavelength, lambdas[0]) * scale,
				AtmospherePrecomputer::Interpolate(wavelengths, scatteringWavelength, lambdas[1]) * scale,
				AtmospherePrecomputer::Interpolate(wavelengths, scatteringWavelength, lambdas[2]) * scale);
		};
		//
		auto ProcessDensityLayers = [lengthUnit](std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>& processedArray,
			const std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>& layersIn) -> void
		{
			for (unsigned int i = 0; i < MAX_DENSITY_PROFILE_LAYERS; i++)
			{
				if (i + 1 > layersIn.size())
				{
					processedArray[MAX_DENSITY_PROFILE_LAYERS - 1 - i] = DensityProfileLayer();
					continue;
				}

				processedArray[MAX_DENSITY_PROFILE_LAYERS - 1 - i] = layersIn[i];
				DensityProfileLayer& currentLayer = processedArray[MAX_DENSITY_PROFILE_LAYERS - 1 - i];
				currentLayer.width /= lengthUnit;
				currentLayer.exp_scale *= lengthUnit;
				currentLayer.linear_term *= lengthUnit;
			}
		};

		glm::dvec3 sunK;
		if (precomputeIlluminance)
		{
			sunK = glm::vec3(MAX_LUMINOUS_EFFICACY);
		}
		else {
			AtmospherePrecomputer::ComputeSpectralRadianceToLuminanceFactors(wavelengths, solarIrradiance,
				-3 /* lambda_power */, &sunK.r, &sunK.g, &sunK.b);
		}

		glm::dvec3 skyK;
		AtmospherePrecomputer::ComputeSpectralRadianceToLuminanceFactors(wavelengths, solarIrradiance,
			0 /* lambda_power */, &skyK.r, &skyK.g, &skyK.b);

		ProcessedAtmoData pAtmoData;
		pAtmoData.solarIrradiance = InterpolateScatteringSpectrum(solarIrradiance, 1.0);
		pAtmoData.bottomRadius = scatteringData.kBottomRadius / lengthUnit;
		pAtmoData.topRadius = scatteringData.kTopRadius / lengthUnit;
		pAtmoData.rayleighScattering = InterpolateScatteringSpectrum(rayleighScattering, lengthUnit);
		pAtmoData.mieScattering = InterpolateScatteringSpectrum(mieScattering, lengthUnit);
		pAtmoData.mieExtinction = InterpolateScatteringSpectrum(mieExtinction, lengthUnit);
		pAtmoData.miePhaseFunctionG = scatteringData.kMiePhaseFunctionG;
		pAtmoData.absorptionExtinction = InterpolateScatteringSpectrum(absorptionExtinction, lengthUnit);
		pAtmoData.groundAlbdeo = InterpolateScatteringSpectrum(groundAlbedo, 1.0);
		pAtmoData.sunAngularRadius = (float)SUN_ANGULAR_RADIUS;
		pAtmoData.muSMin = cos((halfPrecision ? 102.0 : 120.0) / 180.0 * kPi); //muSMin
		pAtmoData.sunSpectralRadianceToLuminance = sunK;
		pAtmoData.skySpectralRadianceToLuminance = skyK;

		ProcessDensityLayers(pAtmoData.rayleighDensity, scatteringData.rayleighDensity);
		ProcessDensityLayers(pAtmoData.mieDensity, scatteringData.mieDensity);
		ProcessDensityLayers(pAtmoData.absorptionDensity, scatteringData.absorptionDensity);

		return pAtmoData;
	}

	void AtmospherePrecomputer::ConvertSpectrumToLinearSrgb(const std::vector<double>& wavelengths, const std::vector<double>& spectrum, double* r, double* g, double* b)
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		const int dlambda = 1;
		for (unsigned int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda)
		{
			double value = Interpolate(wavelengths, spectrum, lambda);
			x += CieColorMatchingFunctionTableValue(lambda, 1) * value;
			y += CieColorMatchingFunctionTableValue(lambda, 2) * value;
			z += CieColorMatchingFunctionTableValue(lambda, 3) * value;
		}
		*r = MAX_LUMINOUS_EFFICACY *
			(XYZ_TO_SRGB[0] * x + XYZ_TO_SRGB[1] * y + XYZ_TO_SRGB[2] * z) * dlambda;
		*g = MAX_LUMINOUS_EFFICACY *
			(XYZ_TO_SRGB[3] * x + XYZ_TO_SRGB[4] * y + XYZ_TO_SRGB[5] * z) * dlambda;
		*b = MAX_LUMINOUS_EFFICACY *
			(XYZ_TO_SRGB[6] * x + XYZ_TO_SRGB[7] * y + XYZ_TO_SRGB[8] * z) * dlambda;
	}

#pragma region Atmosphere Precomputer

	namespace
	{
		// Half floats are filterable as render targets on every backend and are precise enough
		// for the precomputed values (Bruneton's reference implementation also uses them)
		constexpr DH::Format PRECOMPUTE_FORMAT = DH::Format::RGBA16F;

		sRef<DH::Texture2D> CreateTarget2D(int width, int height)
		{
			return DH::Texture2D::Create({ (size_t)width, (size_t)height }, nullptr, PRECOMPUTE_FORMAT,
										 DH::TextureUsage::SampledRenderTarget);
		}

		sRef<DH::Texture3D> CreateTarget3D(int width, int height, int depth)
		{
			return DH::Texture3D::Create({ (size_t)width, (size_t)height, (size_t)depth }, nullptr, PRECOMPUTE_FORMAT,
										 DH::TextureUsage::SampledRenderTarget);
		}

		DH::BindGroupLayoutEntry FragmentTexture(uint32_t binding, bool is3D = false)
		{
			return { binding,
					 is3D ? DH::ShaderResourceType::CombinedTextureSampler3D : DH::ShaderResourceType::CombinedTextureSampler,
					 DH::ShaderStage::Fragment };
		}
	}

	// GPU objects only needed while precomputing
	struct AtmospherePrecomputer::PrecomputeResources
	{
		// Temporary textures. deltaMultipleScatteringTexture is only needed for scattering order 3
		// or more, while deltaRayleighScatteringTexture is only needed for double scattering, so they
		// share one texture.
		sRef<DH::Texture2D> deltaIrradianceTexture;
		sRef<DH::Texture3D> deltaRayleighScatteringTexture;
		sRef<DH::Texture3D> deltaMieScatteringTexture;
		sRef<DH::Texture3D> deltaScatteringDensityTexture;
		sRef<DH::Texture3D>& deltaMultipleScatteringTexture = deltaRayleighScatteringTexture;

		// Render targets. 3D textures are rendered one depth slice (layer) at a time.
		sRef<DH::Framebuffer> transmittanceTarget;
		sRef<DH::Framebuffer> irradianceTarget;                  // delta irradiance, irradiance
		std::vector<sRef<DH::Framebuffer>> singleScatteringTargets;   // delta Rayleigh, delta Mie, scattering
		std::vector<sRef<DH::Framebuffer>> scatteringDensityTargets;  // delta scattering density
		std::vector<sRef<DH::Framebuffer>> multipleScatteringTargets; // delta multiple scattering, scattering

		sRef<DH::GraphicsPipeline> transmittancePipeline;
		sRef<DH::GraphicsPipeline> directIrradiancePipeline;
		sRef<DH::GraphicsPipeline> singleScatteringPipeline;
		sRef<DH::GraphicsPipeline> scatteringDensityPipeline;
		sRef<DH::GraphicsPipeline> indirectIrradiancePipeline;
		sRef<DH::GraphicsPipeline> multipleScatteringPipeline;

		// Set 0: the atmosphere. Set 1: each pass's input textures.
		sRef<DH::BindGroup> atmosphereGroup;
		sRef<DH::BindGroup> transmittanceInputGroup;
		sRef<DH::BindGroup> scatteringDensityInputGroup;
		sRef<DH::BindGroup> indirectIrradianceInputGroup;
		sRef<DH::BindGroup> multipleScatteringInputGroup;
	};

	AtmospherePrecomputer::AtmospherePrecomputer(const ScatteringData& atmoData, double lengthUnitInMeters)
		: AtmospherePrecomputer(ProcessAtmosphericData(atmoData, lengthUnitInMeters), lengthUnitInMeters)
	{
	}

	AtmospherePrecomputer::AtmospherePrecomputer(const ProcessedAtmoData& atmoData, double lengthUnitInMetresIn) :
		processedData(atmoData),
		lengthUnitInMetres(lengthUnitInMetresIn)
	{
		const AtmosphereUniforms uniforms = PackUniforms(processedData);
		uniformBuffer = DH::Buffer::Create({ sizeof(AtmosphereUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Static }, &uniforms);

		// Allocate the precomputed textures, but don't precompute them yet
		transmittanceTexture = CreateTarget2D(TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);
		scatteringTexture = CreateTarget3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);
		irradianceTexture = CreateTarget2D(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);
	}

	AtmospherePrecomputer::~AtmospherePrecomputer() = default;

	AtmosphereUniforms AtmospherePrecomputer::PackUniforms(const ProcessedAtmoData& data)
	{
		AtmosphereUniforms uniforms{};
		uniforms.solarIrradiance = glm::vec4(data.solarIrradiance, data.sunAngularRadius);
		uniforms.rayleighScattering = glm::vec4(data.rayleighScattering, data.bottomRadius);
		uniforms.mieScattering = glm::vec4(data.mieScattering, data.topRadius);
		uniforms.mieExtinction = glm::vec4(data.mieExtinction, data.miePhaseFunctionG);
		uniforms.absorptionExtinction = glm::vec4(data.absorptionExtinction, data.muSMin);
		uniforms.groundAlbedo = glm::vec4(data.groundAlbdeo, 0.0f);

		float constants[MAX_DENSITY_PROFILE_LAYERS * 3] = {};
		const std::array<const std::array<DensityProfileLayer, MAX_DENSITY_PROFILE_LAYERS>*, 3> profiles = {
			&data.rayleighDensity, &data.mieDensity, &data.absorptionDensity
		};
		for (unsigned int p = 0; p < 3; p++)
		{
			for (unsigned int l = 0; l < MAX_DENSITY_PROFILE_LAYERS; l++)
			{
				const DensityProfileLayer& layer = (*profiles[p])[l];
				const unsigned int index = p * MAX_DENSITY_PROFILE_LAYERS + l;
				uniforms.densityLayers[index] = glm::vec4(layer.width, layer.exp_term, layer.exp_scale, layer.linear_term);
				constants[index] = (float)layer.constant_term;
			}
		}
		uniforms.densityConstants[0] = glm::vec4(constants[0], constants[1], constants[2], constants[3]);
		uniforms.densityConstants[1] = glm::vec4(constants[4], constants[5], 0.0f, 0.0f);
		return uniforms;
	}

	void AtmospherePrecomputer::PrecomputeTextures(DH::CommandList& cmd, const SceneContext& context, unsigned int numScatteringOrders)
	{
		precomputeResources = umake<PrecomputeResources>();
		PrecomputeResources& res = *precomputeResources;

		res.deltaIrradianceTexture = CreateTarget2D(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);
		res.deltaRayleighScatteringTexture = CreateTarget3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);
		res.deltaMieScatteringTexture = CreateTarget3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);
		res.deltaScatteringDensityTexture = CreateTarget3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH);

		auto CreateTarget = [](std::vector<DH::FramebufferAttachment> attachments)
		{
			DH::FramebufferSpecification spec;
			spec.attachments = std::move(attachments);
			return DH::Framebuffer::Create(spec);
		};

		res.transmittanceTarget = CreateTarget({ transmittanceTexture });
		res.irradianceTarget = CreateTarget({ res.deltaIrradianceTexture, irradianceTexture });
		for (uint32_t layer = 0; layer < (uint32_t)SCATTERING_TEXTURE_DEPTH; layer++)
		{
			res.singleScatteringTargets.push_back(CreateTarget({
				{ res.deltaRayleighScatteringTexture, 0, layer },
				{ res.deltaMieScatteringTexture, 0, layer },
				{ scatteringTexture, 0, layer } }));
			res.scatteringDensityTargets.push_back(CreateTarget({ { res.deltaScatteringDensityTexture, 0, layer } }));
			res.multipleScatteringTargets.push_back(CreateTarget({
				{ res.deltaMultipleScatteringTexture, 0, layer },
				{ scatteringTexture, 0, layer } }));
		}

		// Bind groups
		const sRef<DH::BindGroupLayout> atmosphereLayout = DH::BindGroupLayout::Create(
			{ { 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::Fragment } });
		const sRef<DH::BindGroupLayout> transmittanceInputLayout = DH::BindGroupLayout::Create({ FragmentTexture(0) });
		const sRef<DH::BindGroupLayout> scatteringDensityInputLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0), FragmentTexture(1, true), FragmentTexture(2, true), FragmentTexture(3, true), FragmentTexture(4) });
		const sRef<DH::BindGroupLayout> indirectIrradianceInputLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0, true), FragmentTexture(1, true), FragmentTexture(2, true) });
		const sRef<DH::BindGroupLayout> multipleScatteringInputLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0), FragmentTexture(1, true) });

		const sRef<DH::Sampler>& sampler = context.clampSampler;
		using Entry = DH::BindGroupEntry;
		res.atmosphereGroup = DH::BindGroup::Create(atmosphereLayout, { Entry::UniformBuffer(0, uniformBuffer) });
		res.transmittanceInputGroup = DH::BindGroup::Create(transmittanceInputLayout,
			{ Entry::TextureSampler(0, transmittanceTexture, sampler) });
		res.scatteringDensityInputGroup = DH::BindGroup::Create(scatteringDensityInputLayout, {
			Entry::TextureSampler(0, transmittanceTexture, sampler),
			Entry::TextureSampler(1, res.deltaRayleighScatteringTexture, sampler),
			Entry::TextureSampler(2, res.deltaMieScatteringTexture, sampler),
			Entry::TextureSampler(3, res.deltaMultipleScatteringTexture, sampler),
			Entry::TextureSampler(4, res.deltaIrradianceTexture, sampler) });
		res.indirectIrradianceInputGroup = DH::BindGroup::Create(indirectIrradianceInputLayout, {
			Entry::TextureSampler(0, res.deltaRayleighScatteringTexture, sampler),
			Entry::TextureSampler(1, res.deltaMieScatteringTexture, sampler),
			Entry::TextureSampler(2, res.deltaMultipleScatteringTexture, sampler) });
		res.multipleScatteringInputGroup = DH::BindGroup::Create(multipleScatteringInputLayout, {
			Entry::TextureSampler(0, transmittanceTexture, sampler),
			Entry::TextureSampler(1, res.deltaScatteringDensityTexture, sampler) });

		// Pipelines: full screen triangles, no depth. Attachments that accumulate the sum of every
		// scattering order use additive (One + One) blending.
		const sRef<DH::ShaderModule> fullscreenShader = Shaders::Create("Fullscreen.vert");
		auto CreatePipeline = [&](const char* fragmentShader, const sRef<DH::Framebuffer>& target,
								  const sRef<DH::BindGroupLayout>& inputLayout, DH::BlendDesc blend = {})
		{
			DH::GraphicsPipelineDesc desc;
			desc.vertexShader = fullscreenShader;
			desc.fragmentShader = Shaders::Create(fragmentShader);
			desc.depth = { false, false, DH::CompareOp::Always };
			desc.blend = std::move(blend);
			desc.targetLayout = target->GetLayout();
			desc.bindGroupLayouts = { atmosphereLayout };
			if (inputLayout != nullptr)
				desc.bindGroupLayouts.push_back(inputLayout);
			desc.pushConstantSize = sizeof(PrecomputePushConstants);
			return DH::GraphicsPipeline::Create(desc);
		};
		using DH::BlendPreset;
		res.transmittancePipeline = CreatePipeline("Transmittance.frag", res.transmittanceTarget, nullptr);
		res.directIrradiancePipeline = CreatePipeline("DirectIrradiance.frag", res.irradianceTarget, transmittanceInputLayout);
		res.singleScatteringPipeline = CreatePipeline("SingleScattering.frag", res.singleScatteringTargets[0], transmittanceInputLayout);
		res.scatteringDensityPipeline = CreatePipeline("ScatteringDensity.frag", res.scatteringDensityTargets[0], scatteringDensityInputLayout);
		res.indirectIrradiancePipeline = CreatePipeline("IndirectIrradiance.frag", res.irradianceTarget, indirectIrradianceInputLayout,
			{ BlendPreset::None, BlendPreset::AdditiveOne });
		res.multipleScatteringPipeline = CreatePipeline("MultipleScattering.frag", res.multipleScatteringTargets[0], multipleScatteringInputLayout,
			{ BlendPreset::None, BlendPreset::AdditiveOne });

		// Precomputing 3 wavelengths stores radiance directly, so no conversion to luminance
		PrecomputePushConstants pushConstants;
		pushConstants.luminanceFromRadiance = glm::mat4(1.0f);

		auto Draw = [&](const sRef<DH::Framebuffer>& target, DH::LoadOp loadOp, const sRef<DH::GraphicsPipeline>& pipeline,
						const sRef<DH::BindGroup>& inputs)
		{
			DH::RenderPassDesc pass;
			pass.target = target.get();
			pass.colourLoadOp = loadOp;
			pass.clearColour = glm::vec4(0.0f);
			pass.depthLoadOp = DH::LoadOp::DontCare;
			cmd.BeginRenderPass(pass);
			cmd.BindPipeline(*pipeline);
			cmd.BindGroup(0, *res.atmosphereGroup);
			if (inputs != nullptr)
				cmd.BindGroup(1, *inputs);
			cmd.PushConstants(pushConstants);
			cmd.Draw(3);
			cmd.EndRenderPass();
		};

		// Compute the transmittance, and store it in transmittanceTexture
		Draw(res.transmittanceTarget, DH::LoadOp::Clear, res.transmittancePipeline, nullptr);

		// Compute the direct irradiance, store it in deltaIrradianceTexture and initialise
		// irradianceTexture with zeros (it only stores the irradiance from the sky)
		Draw(res.irradianceTarget, DH::LoadOp::Clear, res.directIrradiancePipeline, res.transmittanceInputGroup);

		// Compute the Rayleigh and Mie single scattering, store them in deltaRayleighScatteringTexture
		// and deltaMieScatteringTexture, and store them combined in scatteringTexture
		for (uint32_t layer = 0; layer < (uint32_t)SCATTERING_TEXTURE_DEPTH; layer++)
		{
			pushConstants.layer = (int32_t)layer;
			Draw(res.singleScatteringTargets[layer], DH::LoadOp::Clear, res.singleScatteringPipeline, res.transmittanceInputGroup);
		}

		// Compute the 2nd, 3rd and 4th order of scattering, in sequence
		for (unsigned int scatteringOrder = 2; scatteringOrder <= numScatteringOrders; ++scatteringOrder)
		{
			// Compute the scattering density, and store it in deltaScatteringDensityTexture
			pushConstants.scatteringOrder = (int32_t)scatteringOrder;
			for (uint32_t layer = 0; layer < (uint32_t)SCATTERING_TEXTURE_DEPTH; layer++)
			{
				pushConstants.layer = (int32_t)layer;
				Draw(res.scatteringDensityTargets[layer], DH::LoadOp::Clear, res.scatteringDensityPipeline, res.scatteringDensityInputGroup);
			}

			// Compute the indirect irradiance, store it in deltaIrradianceTexture and accumulate it
			// in irradianceTexture
			pushConstants.scatteringOrder = (int32_t)scatteringOrder - 1;
			Draw(res.irradianceTarget, DH::LoadOp::Load, res.indirectIrradiancePipeline, res.indirectIrradianceInputGroup);

			// Compute the multiple scattering, store it in deltaMultipleScatteringTexture, and
			// accumulate it in scatteringTexture
			for (uint32_t layer = 0; layer < (uint32_t)SCATTERING_TEXTURE_DEPTH; layer++)
			{
				pushConstants.layer = (int32_t)layer;
				Draw(res.multipleScatteringTargets[layer], DH::LoadOp::Load, res.multipleScatteringPipeline, res.multipleScatteringInputGroup);
			}
		}

		precomputed = true;
	}

	void AtmospherePrecomputer::ReleasePrecomputeResources()
	{
		if (precomputeResources == nullptr)
			return;
		DH::GPUContext::Get()->WaitIdle();
		precomputeResources.reset();
	}

#pragma endregion
}
