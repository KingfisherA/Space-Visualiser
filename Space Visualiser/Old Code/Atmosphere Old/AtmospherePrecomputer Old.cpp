#include"SV/Precompiled Global.h"
#include "AtmospherePrecomputer.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include<unordered_map>
#include<string>

#include"SV/OpenGL/OpenGL Context.h"
#include"SV/OpenGL/VAO.h"
#include"SV/OpenGL/VBO.h"
#include"SV/OpenGL/ShaderProgram.h"
#include"SV/Utilities/Geometry.h"
#include"SV/OpenGL/FBO.h"
#include"SV/Utilities/String.h"
#include"SV/OpenGL/Texture.h"

namespace SV::Atmosphere
{
	//
#pragma region Atmosphere Precomputer
	//
	SV::String AtmospherePrecomputer::textureSizeHeader;

	void AtmospherePrecomputer::DrawQuad(const std::initializer_list<bool>& enable_blend)
	{
		unsigned int i = 0;
		for (; i < enable_blend.size(); ++i) {
			if (enable_blend.begin()[i]) {
				glEnablei(GL_BLEND, i);
			}
		}

		fullScreenQuad->Bind();
		glDrawElements(GL_TRIANGLES, fullScreenQuad->GetEleCount(), GL_UNSIGNED_INT, nullptr);
		fullScreenQuad->Unbind();

		for (i = 0; i < enable_blend.size(); ++i) {
			glDisablei(GL_BLEND, i);
		}
	}

	constexpr int kLambdaMin = 360;
	constexpr int kLambdaMax = 830;

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
		int dlambda = 1;
		for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda) {
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

	AtmospherePrecomputer::AtmospherePrecomputer(const WavelengthData& atmoData, double lengthUnitInMetres, Ref<SV::VAO> fullScreenQuadIn, unsigned int numPrecomputedWavelengths) :
		fullScreenQuad(fullScreenQuadIn),
		rawData(atmoData),
		processedData(AtmospherePrecomputer::ProcessAtmosphericData(atmoData, lengthUnitInMetres, numPrecomputedWavelengths > 3)),
		lengthUnitInMetres(lengthUnitInMetres),
		precomputedWavelengths(numPrecomputedWavelengths)
	{
		if (textureSizeHeader == "")
		{
			RecompileTextureHeader();
		}

		atmosphericDataString = AtmospherePrecomputer::CompileAtmosphereDataToString(processedData);
		// Allocate the precomputed textures, but don't precompute them yet.
		transmittanceTexture = create <SV::Texture2D>(
			TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT, SV::TexFormat::RGBA32F);
		scatteringTexture = create <SV::Texture3D>(
			SCATTERING_TEXTURE_WIDTH,
			SCATTERING_TEXTURE_HEIGHT,
			SCATTERING_TEXTURE_DEPTH,
			COMBINE_SCATTERING_TEXTURES ? SV::TexFormat::RGBA32F : SV::TexFormat::RGB32F);
		if (COMBINE_SCATTERING_TEXTURES)
		{
			singleMieScatteringTexture = nullptr;
		}
		else
		{
			singleMieScatteringTexture = create <SV::Texture3D>(
				SCATTERING_TEXTURE_WIDTH,
				SCATTERING_TEXTURE_HEIGHT,
				SCATTERING_TEXTURE_DEPTH,
				COMBINE_SCATTERING_TEXTURES ? SV::TexFormat::RGBA32F : SV::TexFormat::RGB32F);
		}
		irradianceTexture = create <SV::Texture2D>(
			IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT, SV::TexFormat::RGBA32F);
	}

	ProcessedAtmoData AtmospherePrecomputer::ProcessAtmosphericData(const WavelengthData& rawData, const double lengthUnit, const bool precomputeIlluminance, const glm::vec3& lambdas)
	{
		auto InterpolateScatteringSpectrum = [&rawData, &lambdas](const std::vector<double>& scatteringWavelength, double scale) -> glm::vec3
		{
			return glm::vec3(AtmospherePrecomputer::Interpolate(rawData.wavelengths, scatteringWavelength, lambdas[0]) * scale,
				AtmospherePrecomputer::Interpolate(rawData.wavelengths, scatteringWavelength, lambdas[1]) * scale,
				AtmospherePrecomputer::Interpolate(rawData.wavelengths, scatteringWavelength, lambdas[2]) * scale);
		};
		//
		auto ProcessDensityLayers = [lengthUnit](DensityProfileLayer* processedArray, const std::vector<DensityProfileLayer>& layersIn) -> void
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
			AtmospherePrecomputer::ComputeSpectralRadianceToLuminanceFactors(rawData.wavelengths, rawData.solarIrradiance,
				-3 /* lambda_power */, &sunK.r, &sunK.g, &sunK.b);
		}

		glm::dvec3 skyK;
		AtmospherePrecomputer::ComputeSpectralRadianceToLuminanceFactors(rawData.wavelengths, rawData.solarIrradiance,
			0 /* lambda_power */, &skyK.r, &skyK.g, &skyK.b);

		ProcessedAtmoData pAtmoData(InterpolateScatteringSpectrum(rawData.solarIrradiance, 1.0),
			(const float)rawData.sunAngularRadius,
			(const float)rawData.bottomRadius / (const float)lengthUnit,
			(const float)rawData.topRadius / (const float)lengthUnit,
			InterpolateScatteringSpectrum(rawData.rayleighScattering, lengthUnit) , //glm::vec3(5.5e-6, 13.0e-6, 22.4e-6)
			InterpolateScatteringSpectrum(rawData.mieScattering, lengthUnit),
			InterpolateScatteringSpectrum(rawData.mieExtinction, lengthUnit),
			(const float)rawData.miePhaseFunctionG,
			InterpolateScatteringSpectrum(rawData.absorptionExtinction, lengthUnit),
			InterpolateScatteringSpectrum(rawData.groundAlbedo, 1.0),
			cos((const float)rawData.maxSunZenithAngle), //muSMin
			sunK,
			skyK);

		ProcessDensityLayers(&pAtmoData.rayleighDensity[0], rawData.rayleighDensity);
		ProcessDensityLayers(&pAtmoData.mieDensity[0], rawData.mieDensity);
		ProcessDensityLayers(&pAtmoData.absorptionDensity[0], rawData.absorptionDensity);

		return pAtmoData;
	}

	SV::String   AtmospherePrecomputer::CompileAtmosphereHeaderToString(const SV::Dimension2D transmittanceTexDimensions, const glm::uivec4& scatteringTexProperties,
		const SV::Dimension2D irradianceTexDimensions, const bool texturesCombined)
	{
		return SV::String(
			"const int TRANSMITTANCE_TEXTURE_WIDTH = " +
			SV::NumberToString(transmittanceTexDimensions.width) + ";\n" +
			"const int TRANSMITTANCE_TEXTURE_HEIGHT = " +
			SV::NumberToString(transmittanceTexDimensions.height) + ";\n" +
			"const int SCATTERING_TEXTURE_R_SIZE = " +
			SV::NumberToString(scatteringTexProperties.x) + ";\n" +
			"const int SCATTERING_TEXTURE_MU_SIZE = " +
			SV::NumberToString(scatteringTexProperties.y) + ";\n" +
			"const int SCATTERING_TEXTURE_MU_S_SIZE = " +
			SV::NumberToString(scatteringTexProperties.z) + ";\n" +
			"const int SCATTERING_TEXTURE_NU_SIZE = " +
			SV::NumberToString(scatteringTexProperties.w) + ";\n" +
			"const int IRRADIANCE_TEXTURE_WIDTH = " +
			SV::NumberToString(irradianceTexDimensions.width) + ";\n" +
			"const int IRRADIANCE_TEXTURE_HEIGHT = " +
			SV::NumberToString(irradianceTexDimensions.height) + ";\n" +
			(texturesCombined ?
				"#define COMBINED_SCATTERING_TEXTURES\n" : ""));
	}

	SV::String  AtmospherePrecomputer::CompileAtmosphereDataToString(const ProcessedAtmoData& data)
	{
		auto DensityProfileToString = [](const DensityProfileLayer* layers) -> SV::String
		{
			SV::String  returnVal = "DensityProfile(DensityProfileLayer[" + SV::NumberToString(MAX_DENSITY_PROFILE_LAYERS) + "](";
			for (int i = 0; i < MAX_DENSITY_PROFILE_LAYERS; i++)
			{
				returnVal += "DensityProfileLayer(" +
					SV::NumberToString(layers[i].width) + "," +
					SV::NumberToString(layers[i].exp_term) + "," +
					SV::NumberToString(layers[i].exp_scale) + "," +
					SV::NumberToString(layers[i].linear_term) + "," +
					SV::NumberToString(layers[i].constant_term) + ")";
				returnVal += i < MAX_DENSITY_PROFILE_LAYERS - 1 ? "," : "))";
			}
			return returnVal;
		};

		return "\nconst AtmosphereParameters ATMOSPHERE = AtmosphereParameters(\n" +
			SV::VectorToString(data.solarIrradiance) + ",\n" +
			SV::NumberToString(data.sunAngularRadius) + ",\n" +
			SV::NumberToString(data.bottomRadius) + ",\n" +
			SV::NumberToString(data.topRadius) + ",\n" +
			DensityProfileToString(data.rayleighDensity) + ",\n" +
			SV::VectorToString(data.rayleighScattering) + ",\n" +
			DensityProfileToString(data.mieDensity) + ",\n" +
			SV::VectorToString(data.mieScattering) + ",\n" +
			SV::VectorToString(data.mieExtinction) + ",\n" +
			SV::NumberToString(data.miePhaseFunctionG) + ",\n" +
			DensityProfileToString(data.absorptionDensity) + ",\n" +
			SV::VectorToString(data.absorptionExtinction) + ",\n" +
			SV::VectorToString(data.groundAlbdeo) + ",\n" +
			SV::NumberToString(data.muSMin) + ");\n" +
			"const vec3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = " + SV::VectorToString(data.skySpectralRadianceToLuminance) + ";\n" +
			"const vec3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = " + SV::VectorToString(data.skySpectralRadianceToLuminance) + ";\n";
	}

	void AtmospherePrecomputer::RecompileTextureHeader()
	{
		textureSizeHeader = AtmospherePrecomputer::CompileAtmosphereHeaderToString({ TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT },
			{ SCATTERING_TEXTURE_R_SIZE, SCATTERING_TEXTURE_MU_SIZE, SCATTERING_TEXTURE_MU_S_SIZE, SCATTERING_TEXTURE_NU_SIZE },
			{ IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT }, COMBINE_SCATTERING_TEXTURES);
	}

	void AtmospherePrecomputer::PrecomputeTextures(unsigned int numScatteringOrders)
	{
		bool isBlendEnabled = glIsEnabled(GL_BLEND);

		// The precomputations require temporary textures, in particular to store the
		// contribution of one scattering order, which is needed to compute the next
		// order of scattering (the final precomputed textures store the sum of all
		// the scattering orders). We allocate them here, and destroy them at the end
		// of this method.
		SV::Texture2D delta_irradiance_texture(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT, SV::TexFormat::RGBA32F);

		SV::Texture3D deltaRayleighScatteringTexture(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, SV::TexFormat::RGBA32F);

		SV::Texture3D deltaMieScatteringTexture(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, SV::TexFormat::RGBA32F);

		SV::Texture3D deltaScatteringDensityTexture(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, SV::TexFormat::RGBA32F);

		// deltaMultipleScatteringTexture is only needed to compute scattering
		// order 3 or more, while deltaRayleighScatteringTexture and
		// deltaMieScatteringTexture are only needed to compute double scattering.
		// Therefore, to save memory, we can store deltaRayleighScatteringTexture
		// and deltaMultipleScatteringTexture in the same GPU texture.
		SV::Texture3D& deltaMultipleScatteringTexture = deltaRayleighScatteringTexture;

		// The precomputations also require a temporary framebuffer object, created
		// here (and destroyed at the end of this method).
		SV::FBO fbo;

		// The actual precomputations depend on whether we want to store precomputed
		// irradiance or illuminance values.
		if (precomputedWavelengths <= 3)
		{
			//glm::vec3 lambdas{ kLambdaR, kLambdaG, kLambdaB };
			glm::mat3 luminance_from_radiance{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
			Precompute(fbo, delta_irradiance_texture, deltaRayleighScatteringTexture,
				deltaMieScatteringTexture, deltaScatteringDensityTexture,
				deltaMultipleScatteringTexture, K_LAMBDA, luminance_from_radiance,
				false /* blend */, numScatteringOrders);
		}
		else
		{
			constexpr double kLambdaMin = 360.0;
			constexpr double kLambdaMax = 830.0;
			int num_iterations = (precomputedWavelengths + 2) / 3;
			double dlambda = (kLambdaMax - kLambdaMin) / (3.0 * double(num_iterations));
			for (int i = 0; i < num_iterations; i++)
			{
				glm::vec3 lambdas
				{
				  kLambdaMin + (3 * double(i) + 0.5) * dlambda,
				  kLambdaMin + (3 * double(i) + 1.5) * dlambda,
				  kLambdaMin + (3 * double(i) + 2.5) * dlambda
				};
				auto coeff = [dlambda](double lambda, int component)
				{
					// Note that we don't include MAX_LUMINOUS_EFFICACY here, to avoid
					// artefacts due to too large values when using half precision on GPU.
					// We add this term back in kAtmosphereShader, via
					// SKY_SPECTRAL_RADIANCE_TO_LUMINANCE (see also the comments in the
					// Model constructor).
					double x = CieColorMatchingFunctionTableValue(lambda, 1);
					double y = CieColorMatchingFunctionTableValue(lambda, 2);
					double z = CieColorMatchingFunctionTableValue(lambda, 3);
					return static_cast<float>((
						XYZ_TO_SRGB[component * 3] * x +
						XYZ_TO_SRGB[component * 3 + 1] * y +
						XYZ_TO_SRGB[component * 3 + 2] * z) * dlambda);
				};

				glm::mat3 luminance_from_radiance{
				  coeff(lambdas[0], 0), coeff(lambdas[1], 0), coeff(lambdas[2], 0),
				  coeff(lambdas[0], 1), coeff(lambdas[1], 1), coeff(lambdas[2], 1),
				  coeff(lambdas[0], 2), coeff(lambdas[1], 2), coeff(lambdas[2], 2)
				};

				Precompute(fbo, delta_irradiance_texture,
					deltaRayleighScatteringTexture, deltaMieScatteringTexture,
					deltaScatteringDensityTexture, deltaMultipleScatteringTexture,
					lambdas, luminance_from_radiance, i > 0 /* blend */,
					numScatteringOrders);
			}

			// After the above iterations, the transmittance texture contains the
			// transmittance for the 3 wavelengths used at the last iteration. But we
			// want the transmittance at kLambdaR, kLambdaG, kLambdaB instead, so we
			// must recompute it here for these 3 wavelengths:

			SV::ShaderProgram computeTransmittanceShader(SV::VertexShader(".\\Shaders\\Atmosphere\\Precompute\\PrecomputeVertexShader.v.glsl"), SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeTransmittanceShader.f.glsl", true,
				&(ShaderKeyMap{
					{"atmoParameters", (SV::String)GetAtmosphereDataHeader()}
			})));

			fbo.Bind();
			fbo.AddColourAttachment(0, transmittanceTexture);
			fbo.DrawBuffers({ 0 });
			glViewport(0, 0, TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);

			computeTransmittanceShader.Bind();
			DrawQuad({});

			fbo.Unbind();
		}

		if (isBlendEnabled) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);

		SV::ShaderProgram::Unbind();
		fbo.Unbind();
	}

	void AtmospherePrecomputer::SetProgramUniforms(SV::ShaderProgram& program, const TextureUnits transmittanceUnit, const TextureUnits scatteringUnit, const TextureUnits irradianceUnit,
		const TextureUnits mieScatteringUnit) const
	{
		program.UploadTexture("transmittance_texture", transmittanceUnit, (const SV::ITexture&) * transmittanceTexture);
		program.UploadTexture("scattering_texture", scatteringUnit, (const SV::ITexture&) * scatteringTexture);
		program.UploadTexture("irradiance_texture", irradianceUnit, (const SV::ITexture&) * irradianceTexture);
		if (singleMieScatteringTexture != nullptr) program.UploadTexture("single_mie_scattering_texture", mieScatteringUnit, (const SV::ITexture&) * singleMieScatteringTexture);
	}

	void AtmospherePrecomputer::ConvertSpectrumToLinearSrgb(const std::vector<double>& wavelengths, const std::vector<double>& spectrum, double* r, double* g, double* b)
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		const int dlambda = 1;
		for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda)
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

	void AtmospherePrecomputer::Precompute(SV::FBO& fbo, SV::Texture2D& deltaIrradianceTexture, SV::Texture3D& deltaRayleighScatteringTexture, SV::Texture3D& deltaMieScatteringTexture, SV::Texture3D& deltaScatteringDensityTexture,
		SV::Texture3D& deltaMultipleScatteringTexture, const glm::vec3& lambdas, const  glm::mat3& luminance_from_radiance, bool blend, unsigned int numScatteringOrders)
	{
		// The precomputations require specific GLSL programs, for each precomputation
		// step. We create and compile them here (they are automatically destroyed
		// when this method returns, via the Program destructor).

		ShaderKeyMap programAtmoParams = {
			{"maxDensityProfileLayers", SV::NumberToString(MAX_DENSITY_PROFILE_LAYERS)},
			{"atmoParameters", textureSizeHeader + CompileAtmosphereDataToString(ProcessAtmosphericData(rawData, lengthUnitInMetres, precomputedWavelengths > 3, lambdas))}
		};

		SV::VertexShader vShader(".\\Shaders\\Atmosphere\\Precompute\\PrecomputeVertexShader.v.glsl");
		SV::GeometryShader gShader(".\\Shaders\\Atmosphere\\Precompute\\PrecomputeGeometryShader.g.glsl");

		SV::ShaderProgram transmittanceProgram(vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeTransmittanceShader.f.glsl", true, &programAtmoParams));

		SV::ShaderProgram directIrradianceProgram(vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeDirectIrradianceShader.f.glsl", true, &programAtmoParams));

		SV::ShaderProgram singleScatteringProgram(vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeSingleScatteringShader.f.glsl", true, &programAtmoParams),
			gShader);

		SV::ShaderProgram scatteringDensityProgram(
			vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeScatteringDensityShader.f.glsl", true, &programAtmoParams),
			gShader);

		SV::ShaderProgram indirectIrradianceProgram(
			vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeIndirectIrradianceShader.f.glsl", true, &programAtmoParams));

		SV::ShaderProgram multipleScatteringProgram(
			vShader,
			SV::FragmentShader(".\\Shaders\\Atmosphere\\Precompute\\ComputeMultipleScatteringShader.f.glsl", true, &programAtmoParams),
			gShader);

		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

		// Compute the transmittance, and store it in transmittance_texture_.
		fbo.AddColourAttachment(0, transmittanceTexture);
		fbo.DrawBuffer(0);
		fbo.Bind();

		fbo.SetViewport({ 0,0, TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT });

		transmittanceProgram.Bind();
		DrawQuad({});

		// Compute the direct irradiance, store it in delta_irradiance_texture and,
		// depending on 'blend', either initialize irradianceTexture with zeros or
		// leave it unchanged (we don't want the direct irradiance in
		// irradianceTexture, but only the irradiance from the sky).
		fbo.Reset();
		fbo.AddColourAttachment(0, WeakRef(deltaIrradianceTexture));
		fbo.AddColourAttachment(1, irradianceTexture);
		fbo.DrawBuffers({ 0,1 });

		fbo.SetViewport({ 0,0, IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT });
		directIrradianceProgram.Bind();
		directIrradianceProgram.UploadTexture("transmittance_texture", 0, *transmittanceTexture);

		DrawQuad({ false, blend });

		// Compute the rayleigh and mie single scattering, store them in
		// deltaRayleighScatteringTexture and deltaMieScatteringTexture, and
		// either store them or accumulate them in scatteringTexture and
		// singleMieScatteringTexture.

		fbo.Reset();
		fbo.AddColourAttachment(0, WeakRef(deltaRayleighScatteringTexture));
		fbo.AddColourAttachment(1, WeakRef(deltaMieScatteringTexture));
		fbo.AddColourAttachment(2, scatteringTexture);

		if (singleMieScatteringTexture != 0)
		{
			fbo.AddColourAttachment(3, singleMieScatteringTexture);
			fbo.DrawBuffers({ 0,1 ,2, 3 });
		}
		else
		{
			fbo.DrawBuffers({ 0,1 ,2 });
		}

		fbo.SetViewport({ 0,0,SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT });
		singleScatteringProgram.Bind();
		singleScatteringProgram.UploadMatrixF("luminance_from_radiance", luminance_from_radiance);
		singleScatteringProgram.UploadTexture("transmittance_texture", 0, *transmittanceTexture);

		for (unsigned int layer = 0; layer < SCATTERING_TEXTURE_DEPTH; ++layer)
		{
			singleScatteringProgram.UploadInt("layer", (int)layer);
			DrawQuad({ false, false, blend, blend });
		}

		// Compute the 2nd, 3rd and 4th order of scattering, in sequence.
		for (unsigned int scatteringOrder = 2; scatteringOrder <= numScatteringOrders; ++scatteringOrder)
		{
			// Compute the scattering density, and store it in deltaScatteringDensityTexture.
			fbo.Reset();
			fbo.AddColourAttachment(0, WeakRef(deltaScatteringDensityTexture));
			fbo.DrawBuffer(0);
			fbo.SetViewport({ 0,0, SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT });
			scatteringDensityProgram.Bind();

			scatteringDensityProgram.UploadTexture("transmittance_texture", 0, *transmittanceTexture);
			scatteringDensityProgram.UploadTexture("single_rayleigh_scattering_texture", 1, deltaRayleighScatteringTexture);
			scatteringDensityProgram.UploadTexture("single_mie_scattering_texture", 2, deltaMieScatteringTexture);
			scatteringDensityProgram.UploadTexture("multiple_scattering_texture", 3, deltaMultipleScatteringTexture);
			scatteringDensityProgram.UploadTexture("irradiance_texture", 4, deltaIrradianceTexture);

			scatteringDensityProgram.UploadInt("scattering_order", (int)scatteringOrder);
			for (unsigned int layer = 0; layer < SCATTERING_TEXTURE_DEPTH; ++layer)
			{
				scatteringDensityProgram.UploadInt("layer", (int)layer);
				DrawQuad({});
			}

			// Compute the indirect irradiance, store it in delta_irradiance_texture and
			// accumulate it in irradianceTexture.
			fbo.Reset();
			fbo.AddColourAttachment(0, WeakRef(deltaIrradianceTexture));
			fbo.AddColourAttachment(1, irradianceTexture);
			fbo.DrawBuffers({ 0, 1 });
			fbo.SetViewport({ 0,0, IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT });
			indirectIrradianceProgram.Bind();
			indirectIrradianceProgram.UploadMatrixF("luminance_from_radiance", luminance_from_radiance);
			indirectIrradianceProgram.UploadTexture("single_rayleigh_scattering_texture", 0, deltaRayleighScatteringTexture);
			indirectIrradianceProgram.UploadTexture("single_mie_scattering_texture", 1, deltaMieScatteringTexture);
			indirectIrradianceProgram.UploadTexture("multiple_scattering_texture", 2, deltaMultipleScatteringTexture);

			indirectIrradianceProgram.UploadInt("scattering_order", (int)scatteringOrder - 1);
			DrawQuad({ false, true });

			// Compute the multiple scattering, store it in
			// deltaMultipleScatteringTexture, and accumulate it in
			// scatteringTexture.
			fbo.Reset();
			fbo.AddColourAttachment(0, WeakRef(deltaMultipleScatteringTexture));
			fbo.AddColourAttachment(1, scatteringTexture);
			fbo.DrawBuffers({ 0, 1 });
			fbo.SetViewport({ 0,0, SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT });

			multipleScatteringProgram.Bind();
			multipleScatteringProgram.UploadMatrixF("luminance_from_radiance", luminance_from_radiance);
			multipleScatteringProgram.UploadTexture("transmittance_texture", 0, *transmittanceTexture);
			multipleScatteringProgram.UploadTexture("scattering_density_texture", 1, deltaScatteringDensityTexture);

			for (unsigned int layer = 0; layer < SCATTERING_TEXTURE_DEPTH; ++layer)
			{
				multipleScatteringProgram.UploadInt("layer", (int)layer);
				DrawQuad({ false, true });
			}
		}

		fbo.Reset();
		fbo.Unbind();
	}

#pragma endregion
	//
}