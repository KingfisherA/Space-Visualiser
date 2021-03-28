#include"SV/Precompiled Global.h"
#include "Atmosphere.h"
#include"AtmospherePrecomputer.h"

#include<unordered_map>
#include<string>
#include<utility>
#include<memory>

#include"SV/OpenGL/OpenGL Context.h"
#include"SV/Scene/Camera.h"
#include"SV/Utilities/Geometry.h"

//
//
namespace SV::Atmosphere
{

#pragma region Atmosphere Shader
	//
	AtmosphereShader::AtmosphereShader(const AtmoShaderType type, const SV::String& textureHeader, const Luminance luminance) : SV::ShaderProgram(
		SV::VertexShader([type]() ->SV::StringView {
			switch (type)
			{
			case SV::Atmosphere::AtmoShaderType::SurfaceShader:
				return ".\\Shaders\\Atmosphere\\AtmosphereMain.v.glsl";
				break;
			case SV::Atmosphere::AtmoShaderType::SkyShader:
				return ".\\Shaders\\Atmosphere\\AtmosphereSky.v.glsl";
				break;
			default:
				return"";
				break;
			}}()),
		SV::FragmentShader([type]() ->SV::StringView {
				switch (type)
				{
				case SV::Atmosphere::AtmoShaderType::SurfaceShader:
					return ".\\Shaders\\Atmosphere\\AtmosphereMain.f.glsl";
					break;
				case SV::Atmosphere::AtmoShaderType::SkyShader:
					return ".\\Shaders\\Atmosphere\\AtmosphereSky.f.glsl";
					break;
				default:
					return "";
					break;
				}}(), true, & ShaderKeyMap{
					{"maxDensityProfileLayers", SV::IntegralToString(MAX_DENSITY_PROFILE_LAYERS)},
					{"texProperties", textureHeader},
					//{"atmoParameters", precomputer.GetAtmosphereDataString()},
					{"radianceAPI", /*precomputer.GetPrecomputedWavelengths() > 3 ? "" :*/ "#define RADIANCE_API_ENABLED\n"},
					{ "useLuminance", luminance != Luminance::None ? "#define USE_LUMINANCE\n" : "" }
				})
				), shaderType(type), atmoUniformCache(22, false) {}
				//

				//
#pragma endregion
	//
#pragma region IAtmosphere Interface
//
//
				//IAtmosphere::IAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lumIn) : fullScreenQuad(fullScreenQuadIn), aLuminiance(lumIn) {}
				////
				//void IAtmosphere::Initilise(const std::vector<double>& wavelengths, const std::vector<double>& solarIrradiance, double sunAngularRadius,
				//	double bottomRadius, double topRadius, const std::vector<DensityProfileLayer>& rayleighDensity, const std::vector<double>& rayleighScattering,
				//	const std::vector<DensityProfileLayer>& mieDensity, const std::vector<double>& mieScattering, const std::vector<double>& mieExtinction,
				//	double miePhaseFunctionG, const std::vector<DensityProfileLayer>& absorptionDensity, const std::vector<double>& absorptionExtinction,
				//	const std::vector<double>& groundAlbedo, double maxSunZenithAngle, double lengthUnitInMeters,
				//	bool combineScatteringTextures, int numPrecomputedWavelengths, unsigned int scatteringOrders)
				//{
				//	m_wavelengths = wavelengths;
				//	m_solarIrradiance = solarIrradiance;

				//	if (numPrecomputedWavelengths < 0) numPrecomputedWavelengths = aLuminiance == Luminance::PRECOMPUTED ? 15 : 3;

				//	aPrecomputer = create <SV::Atmosphere::AtmospherePrecomputer>(wavelengths, solarIrradiance, sunAngularRadius, bottomRadius, topRadius,
				//		rayleighDensity, rayleighScattering, mieDensity,
				//		mieScattering, mieExtinction, miePhaseFunctionG, absorptionDensity,
				//		absorptionExtinction, groundAlbedo, maxSunZenithAngle, lengthUnitInMeters, numPrecomputedWavelengths,
				//		combineScatteringTextures, fullScreenQuad);
				//	aPrecomputer->PrecomputeTextures();

				//	auto& atmoParams = AtmosphereShader::GetAtmoProgramParams(aPrecomputer->GetAtmoParamHeader({ aPrecomputer->kLambdaR, aPrecomputer->kLambdaG, aPrecomputer->kLambdaB }),
				//		numPrecomputedWavelengths, aLuminiance, lengthUnitInMeters);

				//	surfaceShader = create <Atmosphere::AtmosphereShader>(atmoParams, AtmoShaderType::SurfaceShader);

				//	surfaceShader->Bind();
				//	surfaceShader->SetTextureUniforms(0, *aPrecomputer->GetTransmittanceTex(), 1, *aPrecomputer->GetScatteringTex(), 2, *aPrecomputer->GetIrradianceTex(),
				//		3, aPrecomputer->GetMieScatteringTex().operator->());
				//	surfaceShader->SetPlanetCentre(glm::vec3(0.0f, 0.0f, 0.0f));
				//	surfaceShader->SetSunSize(sunAngularRadius);
				//	surfaceShader->SetWhitePoint(glm::vec3(1.0f));
				//	surfaceShader->Unbind();

				//	skyShader = create <Atmosphere::AtmosphereShader>(atmoParams, AtmoShaderType::SkyShader);

				//	skyShader->Bind();
				//	skyShader->SetTextureUniforms(0, *aPrecomputer->GetTransmittanceTex(), 1, *aPrecomputer->GetScatteringTex(), 2, *aPrecomputer->GetIrradianceTex(),
				//		3, aPrecomputer->GetMieScatteringTex().operator->());
				//	skyShader->SetPlanetCentre(glm::vec3(0.0f, 0.0f, 0.0f));
				//	skyShader->SetSunSize(sunAngularRadius);
				//	skyShader->SetWhitePoint(glm::vec3(1.0f));
				//	skyShader->Unbind();
				//}
				//
#pragma endregion
	//
#pragma region Earth Atmosphere
				//
				EarthAtmosphere::EarthAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum)
				{

					constexpr bool useHalfPrecision = false;
					constexpr bool useConstantSolarSpectrum = true;
					constexpr bool ozoneEnabled = true;

					// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
					// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
					// summed and averaged in each bin (e.g. the value for 360nm is the average
					// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
					// Values in W.m^-2.
					constexpr int kLambdaMin = 360;
					constexpr int kLambdaMax = 830;
					constexpr unsigned int noOfValues = (kLambdaMax - kLambdaMin) / 10 + 1;
					constexpr double kSolarIrradiance[noOfValues] =
					{
						1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
						1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
						1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
						1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
						1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
						1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
					};

					// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
					// referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
					// each bin (e.g. the value for 360nm is the average of the original values
					// for all wavelengths between 360 and 370nm). Values in m^2.
					constexpr double kOzoneCrossSection[noOfValues] = {
						1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
						8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
						1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
						4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
						2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
						6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
						2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
					};

					// From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
					constexpr double kDobsonUnit = 2.687e20;
					// Maximum number density of ozone molecules, in m^-3 (computed so at to get
					// 300 Dobson units of ozone - for this we divide 300 DU by the integral of
					// the ozone density profile defined below, which is equal to 15km).
					constexpr double kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
					// Wavelength independent solar irradiance "spectrum" (not physically
					// realistic, but was used in the original implementation).
					constexpr double kConstantSolarIrradiance = 1.5;
					constexpr double kBottomRadius = 6360000.0;
					constexpr double kTopRadius = kBottomRadius + 60000.0;

					constexpr double kIndexOfRefTop = 1.0002973; //Refractive index of atmospheric gases at the upper atmosphere (nitrogen at 0.5876 micrometre wavelength at 0 celcius)
					constexpr double kn2T = (kIndexOfRefTop * kIndexOfRefTop) - 1.0; // (n^2 -1)
					constexpr double kRayleighScatteringCoefficient = (0.5 * (kPi * kPi) * kn2T * kn2T); //Rayleigh Scattering Coefficient, without wavelength term and molecular constant (original value: 1.24062e-6)
					constexpr double kRayleighScaleHeight = 8000.0; // Rayleigh layer height for density calculation

					//constexpr double kN = 2.504e25;
					//constexpr double kIndexOfRefSurface = 1.333; //Refractive index of atmospheric gases in the surface (lower) atmosphere (methane at 0.5876 micrometre wavelength at 0 celcius)
					//constexpr double kn2S = (kIndexOfRefSurface * kIndexOfRefSurface) - 1.0;

					//Extinction coefficient is set to zero, as only ozone extincition is considered
					//in this model of Earth's atmosphere.
					 //Calculation: (8.0 * kPi * kPi * kPi * kn2S * kn2S) / (kN * 3.0)
					constexpr double kRayleighExtinctionCoefficient = 0.0;

					constexpr double kMieScaleHeight = 1200.0; //Mie layer height for density calculation
					constexpr double kMieAngstromAlpha = 0.0;
					constexpr double kMieAngstromBeta = 5.328e-3;
					constexpr double kMieSingleScatteringAlbedo = 0.9;
					constexpr double kMiePhaseFunctionG = 0.73;

					constexpr double kGroundAlbedo = 0.1;
					constexpr double maxSunZenithAngle = (useHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

					SV::Atmosphere::DensityProfileLayer rayleighLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
					SV::Atmosphere::DensityProfileLayer mieLayer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);
					// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
					// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
					// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
					// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
					//Specifies the density function for where the extinction coefficient will be applied to
					//In this case, the extinction coefficients are held within the "kOzoneCrossSection" array for
					//extinction caused by ozone gas.
					std::vector<SV::Atmosphere::DensityProfileLayer> ozoneDensity =
					{
						SV::Atmosphere::DensityProfileLayer(25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0),
						SV::Atmosphere::DensityProfileLayer(0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0)
					};

					std::vector<double> wavelengths;
					std::vector<double> solarIrradiance;
					std::vector<double> rayleighScattering;
					std::vector<double> mieScattering;
					std::vector<double> mieExtinction;
					std::vector<double> absorptionExtinction;
					std::vector<double> groundAlbedo;
					for (int j = kLambdaMin; j <= kLambdaMax; j += 10)
					{
						double lambda = static_cast<double>(j) * 1e-3;  // micro-meters
						double lambda4 = pow(lambda, -4);
						double mie = kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
						wavelengths.push_back(j);
						if (useConstantSolarSpectrum)
						{
							solarIrradiance.push_back(kConstantSolarIrradiance);
						}
						else
						{
							solarIrradiance.push_back(kSolarIrradiance[(j - kLambdaMin) / 10]);
						}
						rayleighScattering.push_back(kRayleighScatteringCoefficient * lambda4);
						mieScattering.push_back(mie * kMieSingleScatteringAlbedo);
						mieExtinction.push_back(mie);
						const double ozoneExtinction = ozoneEnabled ? kMaxOzoneNumberDensity * kOzoneCrossSection[(j - kLambdaMin) / 10] : 0.0;
						const double rayleighExtinction = kRayleighExtinctionCoefficient * lambda4 + ozoneExtinction;
						absorptionExtinction.push_back(rayleighExtinction);
						groundAlbedo.push_back(kGroundAlbedo * lambda4);
					}

					SV::Atmosphere::WavelengthData atmoData(wavelengths, solarIrradiance, kSunAngularRadius, kBottomRadius, kTopRadius,
						{ rayleighLayer }, rayleighScattering, { mieLayer }, mieScattering, mieExtinction, kMiePhaseFunctionG,
						ozoneDensity, absorptionExtinction, groundAlbedo, maxSunZenithAngle);

					atmoPrecomputer = make <SV::Atmosphere::AtmospherePrecomputer>(atmoData, kLengthUnitInMeters, fullScreenQuadIn);
					atmoPrecomputer->PrecomputeTextures();

					processedAtmoData = &atmoPrecomputer->GetProcessedAtmoData();
					processedAtmoData->surfaceAtmoIntensity = 0.85f;
				}
				//
#pragma endregion
	//
#pragma region Jupiter Atmosphere
				//
				JupiterAtmosphere::JupiterAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum)
				{
					constexpr bool useHalfPrecision = false;

					constexpr int kLambdaMin = 360;
					constexpr int kLambdaMax = 830;

					// Wavelength independent solar irradiance "spectrum" (not physically
					// realistic, but was used in the original implementation).
					constexpr double kConstantSolarIrradiance = 1.5;
					constexpr double kBottomRadius = 69911000.0;
					constexpr double kTopRadius = kBottomRadius + 5000000.0;

					constexpr double kIndexOfRefTop = 1.00014930; //Refractive index of atmospheric gases at the upper atmosphere (hydrogen at 0.5876 micrometre wavelength at 0 celcius)
					constexpr double kn2T = (kIndexOfRefTop * kIndexOfRefTop) - 1.0; // (n^2 -1)
					constexpr double kRayleighScatteringCoefficient = (0.5 * (kPi * kPi) * kn2T * kn2T); //Rayleigh Scattering Coefficient, without wavelength term and molecular constant (original value: 1.24062e-6)
					constexpr double kRayleighScaleHeight = 30000.0; // Rayleigh layer height for density calculation

					//constexpr double kN = 2.504e25;
					//constexpr double kIndexOfRefSurface = 1.333; //Refractive index of atmospheric gases in the surface (lower) atmosphere (methane at 0.5876 micrometre wavelength at 0 celcius)
					//constexpr double kn2S = (kIndexOfRefSurface * kIndexOfRefSurface) - 1.0;
					constexpr double kRayleighExtinctionCoefficient = 8.5e-8;// 1.5e-6;//9.6654e-7; //Extincition coefficient for methane Calculation: (8.0 * kPi * kPi * kPi * kn2S * kn2S) / (kN * 3.0)

					constexpr double kMieScaleHeight = 1200.0; //Mie layer height for density calculation
					constexpr double kMieAngstromAlpha = 0.0;
					constexpr double kMieAngstromBeta = 5.328e-3;
					constexpr double kMieSingleScatteringAlbedo = 0.9;
					constexpr double kMiePhaseFunctionG = 0.73;

					constexpr double kGroundAlbedo = 1.5;
					constexpr double maxSunZenithAngle =
						(useHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

					SV::Atmosphere::DensityProfileLayer rayleighLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
					SV::Atmosphere::DensityProfileLayer mieLayer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);

					//Specifies the density function for where the extinction coefficient will be applied to
					std::vector<SV::Atmosphere::DensityProfileLayer> absorptionDensityLayers = {
						{0.0, 1.0, -1.0 / ((kTopRadius - kBottomRadius) * 0.1), 0.0, 0.0}
					};

					std::vector<double> wavelengths;
					std::vector<double> solarIrradiance;
					std::vector<double> rayleighScattering;
					std::vector<double> mieScattering;
					std::vector<double> mieExtinction;
					std::vector<double> absorptionExtinction;
					std::vector<double> groundAlbedo;
					for (int j = kLambdaMin; j <= kLambdaMax; j += 10)
					{
						double lambda = static_cast<double>(j) * 1e-3;  // micro-meters
						double lambda4 = pow(lambda, -4);
						double mie = kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
						wavelengths.push_back(j);
						solarIrradiance.push_back(kConstantSolarIrradiance);
						rayleighScattering.push_back(kRayleighScatteringCoefficient * lambda4);
						mieScattering.push_back(mie * kMieSingleScatteringAlbedo);
						mieExtinction.push_back(mie);
						absorptionExtinction.push_back(kRayleighExtinctionCoefficient * lambda4);
						groundAlbedo.push_back(kGroundAlbedo * lambda4);
					}

					SV::Atmosphere::WavelengthData atmoData(wavelengths, solarIrradiance, kSunAngularRadius, kBottomRadius, kTopRadius,
						{ rayleighLayer }, rayleighScattering, { mieLayer }, mieScattering, mieExtinction, kMiePhaseFunctionG,
						absorptionDensityLayers, absorptionExtinction, groundAlbedo, maxSunZenithAngle);

					atmoPrecomputer = make <SV::Atmosphere::AtmospherePrecomputer>(atmoData, kLengthUnitInMeters, fullScreenQuadIn);
					atmoPrecomputer->PrecomputeTextures();

					processedAtmoData = &atmoPrecomputer->GetProcessedAtmoData();
					processedAtmoData->surfaceAtmoIntensity = 0.08f;
				}
				//
#pragma endregion
	//
#pragma region Saturn Atmosphere
				//
				SaturnAtmosphere::SaturnAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum)
				{
					bool use_half_precision_ = true;
					bool use_constant_solar_spectrum_ = false;
					bool use_ozone_ = false;

					// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
					// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
					// summed and averaged in each bin (e.g. the value for 360nm is the average
					// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
					// Values in W.m^-2.
					constexpr int kLambdaMin = 360;
					constexpr int kLambdaMax = 830;
					constexpr double kSolarIrradiance[48] =
					{
						1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
						1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
						1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
						1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
						1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
						1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
					};
					// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
					// referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
					// each bin (e.g. the value for 360nm is the average of the original values
					// for all wavelengths between 360 and 370nm). Values in m^2.
					constexpr double kOzoneCrossSection[48] = {
						1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
						8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
						1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
						4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
						2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
						6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
						2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
					};
					// From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
					constexpr double kDobsonUnit = 2.687e20;
					// Maximum number density of ozone molecules, in m^-3 (computed so at to get
					// 300 Dobson units of ozone - for this we divide 300 DU by the integral of
					// the ozone density profile defined below, which is equal to 15km).
					constexpr double kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
					// Wavelength independent solar irradiance "spectrum" (not physically
					// realistic, but was used in the original implementation).
					constexpr double kConstantSolarIrradiance = 1.5;
					constexpr double kBottomRadius = 6360000.0;
					constexpr double kTopRadius = kBottomRadius + 60000.0;

					constexpr double kIndexOfRef = 1.00014930;//Refractive index of atmospheric gases (hydrogen at 0.28945 micrometre wavelength at 0 celcius)
					constexpr double kn2 = (kIndexOfRef * kIndexOfRef) - 1.0; // (n^2 -1)
					constexpr double kRayleighScatteringCoefficient = (0.5 * (kPi * kPi) * kn2 * kn2); //Rayleigh Scattering Coefficient, without wavelength term and molecular constant
					constexpr double kRayleighScaleHeight = 4000.0;//8000.0;//(((1.38E-23) * 165.0) / (4.02 * 24.79)); //Rayleigh layer scale height for density calculation

					constexpr double kMieScaleHeight = 2300.0;//1326.0; //Mie layer height for density calculation
					constexpr double kMieAngstromAlpha = 0.0; //Angstrom Exponent
					constexpr double kMieAngstromBeta = 0.3; //Mie Optical thickness/depth at wavlength lambda
					constexpr double kMieSingleScatteringAlbedo = 0.9;
					constexpr double kMiePhaseFunctionG = 0.73; //Mie g phase function

					constexpr double kGroundAlbedo = 0.1;
					const double max_sun_zenith_angle =
						(use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;

					SV::Atmosphere::DensityProfileLayer rayleighLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
					SV::Atmosphere::DensityProfileLayer mieLayer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);
					// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
					// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
					// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
					// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
					std::vector<SV::Atmosphere::DensityProfileLayer> ozoneDensity;
					ozoneDensity.push_back(SV::Atmosphere::DensityProfileLayer(25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0));
					ozoneDensity.push_back(SV::Atmosphere::DensityProfileLayer(0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0));

					std::vector<double> wavelengths;
					std::vector<double> solarIrradiance;
					std::vector<double> rayleighScattering;
					std::vector<double> mieScattering;
					std::vector<double> mieExtinction;
					std::vector<double> absorptionExtinction;
					std::vector<double> groundAlbedo;
					for (int j = kLambdaMin; j <= kLambdaMax; j += 10)
					{
						double lambda = static_cast<double>(j) * 1e-3;  // micro-meters
						double mie =
							kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
						wavelengths.push_back(j);
						if (use_constant_solar_spectrum_)
						{
							solarIrradiance.push_back(kConstantSolarIrradiance);
						}
						else
						{
							solarIrradiance.push_back(kSolarIrradiance[(j - kLambdaMin) / 10]);
						}
						rayleighScattering.push_back(kRayleighScatteringCoefficient * pow(lambda, -4));
						mieScattering.push_back(mie * kMieSingleScatteringAlbedo);
						mieExtinction.push_back(mie);
						absorptionExtinction.push_back(use_ozone_ ?
							kMaxOzoneNumberDensity * kOzoneCrossSection[(j - kLambdaMin) / 10] :
							0.0);
						groundAlbedo.push_back(kGroundAlbedo);
					}

					SV::Atmosphere::WavelengthData atmoData(wavelengths, solarIrradiance, kSunAngularRadius, kBottomRadius, kTopRadius,
						{ rayleighLayer }, rayleighScattering, { mieLayer }, mieScattering, mieExtinction, kMiePhaseFunctionG,
						ozoneDensity, absorptionExtinction, groundAlbedo, max_sun_zenith_angle);

					atmoPrecomputer = create <SV::Atmosphere::AtmospherePrecomputer>(atmoData, kLengthUnitInMeters, fullScreenQuadIn);
					atmoPrecomputer->PrecomputeTextures();

					processedAtmoData = &atmoPrecomputer->GetProcessedAtmoData();
					processedAtmoData->surfaceAtmoIntensity = 0.3f;
				}
				//
#pragma endregion
//
#pragma region Titan Atmosphere
				//
				TitanAtmosphere::TitanAtmosphere(Ref<SV::VAO> fullScreenQuadIn, Luminance lum)
				{
					constexpr bool useHalfPrecision = false;

					constexpr int kLambdaMin = 360;
					constexpr int kLambdaMax = 830;

					// Wavelength independent solar irradiance "spectrum" (not physically
					// realistic, but was used in the original implementation).
					constexpr double kConstantSolarIrradiance = 1.5;
					constexpr double kBottomRadius = 2574700.0;
					constexpr double kTopRadius = kBottomRadius + 595457.0;

					constexpr double kIndexOfRefTop = 1.00029433; //Refractive index of atmospheric gases at the upper atmosphere (nitrogen at 0.5876 micrometre wavelength at 0 celcius)
					constexpr double kn2T = (kIndexOfRefTop * kIndexOfRefTop) - 1.0; // (n^2 -1)
					constexpr double kRayleighScatteringCoefficient = (0.5 * (kPi * kPi) * kn2T * kn2T); //Rayleigh Scattering Coefficient, without wavelength term and molecular constant (original value: 1.24062e-6)
					constexpr double kRayleighScaleHeight = 30000.0; // Rayleigh layer height for density calculation

					//constexpr double kN = 2.504e25;
					//constexpr double kIndexOfRefSurface = 1.333; //Refractive index of atmospheric gases in the surface (lower) atmosphere (methane at 0.5876 micrometre wavelength at 0 celcius)
					//constexpr double kn2S = (kIndexOfRefSurface * kIndexOfRefSurface) - 1.0;
					constexpr double kRayleighExtinctionCoefficient = 1.5e-6;//9.6654e-7; //Extincition coefficient for methane Calculation: (8.0 * kPi * kPi * kPi * kn2S * kn2S) / (kN * 3.0)

					constexpr double kMieScaleHeight = 1200.0; //Mie layer height for density calculation
					constexpr double kMieAngstromAlpha = 0.0;
					constexpr double kMieAngstromBeta = 5.328e-3;
					constexpr double kMieSingleScatteringAlbedo = 0.9;
					constexpr double kMiePhaseFunctionG = 0.73;

					constexpr double kGroundAlbedo = 1.5;
					constexpr double maxSunZenithAngle =
						(useHalfPrecision ? 102.0 : 120.0) / 180.0 * kPi;

					SV::Atmosphere::DensityProfileLayer rayleighLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
					SV::Atmosphere::DensityProfileLayer mieLayer(0.0, 1.0, -1.0 / kMieScaleHeight, 0.0, 0.0);

					//Specifies the density function for where the extinction coefficient will be applied to
					std::vector<SV::Atmosphere::DensityProfileLayer> absorptionDensityLayers = {
						{0.0, 1.0, -1.0 / ((kTopRadius - kBottomRadius) * 0.1), 0.0, 0.0}
					};

					std::vector<double> wavelengths;
					std::vector<double> solarIrradiance;
					std::vector<double> rayleighScattering;
					std::vector<double> mieScattering;
					std::vector<double> mieExtinction;
					std::vector<double> absorptionExtinction;
					std::vector<double> groundAlbedo;
					for (int j = kLambdaMin; j <= kLambdaMax; j += 10)
					{
						double lambda = static_cast<double>(j) * 1e-3;  // micro-meters
						double lambda4 = pow(lambda, -4);
						double mie = kMieAngstromBeta / kMieScaleHeight * pow(lambda, -kMieAngstromAlpha);
						wavelengths.push_back(j);
						solarIrradiance.push_back(kConstantSolarIrradiance);
						rayleighScattering.push_back(kRayleighScatteringCoefficient * lambda4);
						mieScattering.push_back(mie * kMieSingleScatteringAlbedo);
						mieExtinction.push_back(mie);
						absorptionExtinction.push_back(kRayleighExtinctionCoefficient * lambda4);
						groundAlbedo.push_back(kGroundAlbedo * lambda4);
					}

					SV::Atmosphere::WavelengthData atmoData(wavelengths, solarIrradiance, kSunAngularRadius, kBottomRadius, kTopRadius,
						{ rayleighLayer }, rayleighScattering, { mieLayer }, mieScattering, mieExtinction, kMiePhaseFunctionG,
						absorptionDensityLayers, absorptionExtinction, groundAlbedo, maxSunZenithAngle);

					atmoPrecomputer = make <SV::Atmosphere::AtmospherePrecomputer>(atmoData, kLengthUnitInMeters, fullScreenQuadIn);
					atmoPrecomputer->PrecomputeTextures();

					processedAtmoData = &atmoPrecomputer->GetProcessedAtmoData();
					processedAtmoData->surfaceAtmoIntensity = 0.9f;
				}
				//
#pragma endregion
//

	//
//#pragma region Atmosphere Renderer
//	//
//				//Static variables
//				SV::Camera AtmosphereRenderer::skyCam;
//				const glm::mat4 AtmosphereRenderer::RING_ROTATION_CORRECTION = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
//				const glm::mat4 AtmosphereRenderer::SKY_SCALE_MAT = glm::scale(glm::vec3(1.01f));
//				//
//				constexpr float ringsInnerRad = 1.5f;
//				constexpr float ringsOuterRad = 3.2f;
//				//
//				AtmosphereRenderer::AtmosphereRenderer() : atmospheres()
//				{
//					sphereVAO = create <SV::VAO>();
//					Ref<SV::Mesh> sphereMesh = SV::Geometry::CreateSphere(1.0f, 50, 50);
//					sphereVAO->Bind();
//					sphereVAO->AddAttributeArray(0, create<SV::ArrayBuffer>(sphereMesh->GetVertexArray()), SV::AttributeLayout(3));
//					sphereVAO->AddAttributeArray(1, create<SV::ArrayBuffer>(sphereMesh->GetNormalsArray()), SV::AttributeLayout(3));
//					sphereVAO->AddAttributeArray(2, create<SV::ArrayBuffer>(sphereMesh->GetTexCoordsArray()), SV::AttributeLayout(2));
//					sphereVAO->AddIndexBuffer(create <SV::IndexBuffer>(sphereMesh->GetIndiciesArray()));
//					sphereVAO->EnableAttributeArray(0, 2);
//					sphereVAO->Unbind();
//
//					ringsVAO = create <SV::VAO>();
//					Ref<SV::Mesh> ringsMesh = SV::Geometry::CreateRings(ringsInnerRad, ringsOuterRad, 50);
//					ringsVAO->Bind();
//					ringsVAO->AddAttributeArray(0, create<SV::ArrayBuffer>(ringsMesh->GetVertexArray()), SV::AttributeLayout(2));
//					ringsVAO->AddIndexBuffer(create <SV::IndexBuffer>(ringsMesh->GetIndiciesArray()));
//					ringsVAO->EnableAttributeArray(0);
//					ringsVAO->Unbind();
//
//					ringsProgram = create <SV::ShaderProgram>(SV::VertexShader("Shaders\\Atmosphere\\Rings\\Rings.v.glsl"), SV::FragmentShader("Shaders\\Atmosphere\\Rings\\Rings.f.glsl"));
//					ringsProgram->Bind();
//					ringsProgram->UploadFloat("ringWidth", ringsOuterRad - ringsInnerRad);
//					ringsProgram->UploadInt("ringMap", 0);
//					ringsProgram->Unbind();
//				}
//				//
//				size_t AtmosphereRenderer::AddAtmosphere(const Ref<IAtmosphere> atmo)
//				{
//					return atmospheres.Add(atmo);
//				}
//				//
//				void AtmosphereRenderer::Render(const glm::mat4& projectionMatrix, const Camera& cam, const float time, const float exposure, const glm::vec3& sunPosition, const SV::ITexture& earthTex,
//					const SV::ITexture& specularMap, const SV::ITexture* cloudsTex, const SV::ITexture* ringsMap, const SV::ITexture* nightMap)
//				{
//					if (atmospheres.GetCount() == 0) return;
//
//					unsigned int i = 0;
//
//					sphereVAO->Bind();
//					for (i = 0; i < atmospheres.GetCount(); i++)
//					{
//						const Ref<IAtmosphere>& currentAtmo = atmospheres[i];
//						const Ref<AtmosphereShader>& surfaceShader = currentAtmo->GetSurfaceShader();
//						const Ref<AtmosphereShader>& skyShader = currentAtmo->GetSkyShader();
//
//						surfaceShader->Bind();
//
//						glm::mat4 transformMat(1.0f);
//						//Upload matricies
//						surfaceShader->UploadMatrixF("uTransformMatrix", transformMat);
//						surfaceShader->UploadMatrixF("uViewProjMatrix", projectionMatrix * cam.GetViewMatrix());
//						surfaceShader->UploadMatrixF("uNormalMatrix", Maths::CalculateNormalMatrix(transformMat));
//						surfaceShader->UploadMatrixF("uInverseViewMatrix", cam.GetInverseViewMatrix());
//
//						//Upload vital uniforms
//						constexpr float planetRadius = 6360.0f;
//						surfaceShader->UploadFloat("uCameraPos", cam.GetActualPosition() * planetRadius);
//						surfaceShader->UploadFloat("uExposure", /*currentAtmo->GetLuminance() != Luminance::NONE ? (float)exposure * (float)1e-5 :*/ (float)exposure);
//						surfaceShader->UploadFloat("uSunPosition", sunPosition);
//						surfaceShader->UploadFloat("uTime", time);
//
//						//Upload textures
//						surfaceShader->UploadTexture("uDiffuseMap", 4, earthTex);
//						surfaceShader->UploadTexture("uSpecularMap", 5, specularMap);
//
//						surfaceShader->UploadOptionalTexture("uCloudsMap", 6, cloudsTex);
//						surfaceShader->UploadOptionalTexture("uRingsShadowMap", 7, ringsMap);
//						surfaceShader->UploadOptionalTexture("uNightMap", 8, nightMap);
//
//						glDrawElements(GL_TRIANGLES, sphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
//
//						skyShader->Bind();
//
//						//Set sky camera properties
//						skyCam.Imitate(cam);
//						constexpr float defaultZoomAmount = 8200.0f;
//						skyCam.SetZoom(defaultZoomAmount * cam.GetZoom());
//						skyCam.SetCentrePosition(glm::vec3(0.0f));
//						skyCam.RecalculateViewMatrix();
//
//						glm::mat4 skyTransformMatrix = SKY_SCALE_MAT;
//						//Upload matricies
//						skyShader->UploadMatrixF("uViewProjMatrix", projectionMatrix * cam.GetViewMatrix());
//						skyShader->UploadMatrixF("uTransformMatrix", skyTransformMatrix);
//						skyShader->UploadMatrixF("uNormalMatrix", Maths::CalculateNormalMatrix(skyTransformMatrix));
//
//						//Upload vital uniforms
//						skyShader->UploadFloat("uCameraPos", cam.GetActualPosition());
//						skyShader->UploadFloat("uExposure", /*currentAtmo->GetLuminance() != Luminance::NONE ? (float)exposure * (float)1e-5 :*/  (float)exposure + 2.0f);
//						skyShader->UploadFloat("uSunPosition", sunPosition);
//
//						glDrawElements(GL_TRIANGLES, sphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
//
//					}
//					sphereVAO->Unbind();
//
//					ringsProgram->Bind();
//					ringsVAO->Bind();
//
//					glm::mat4 ringsTransformation(1.0f);
//					glDisable(GL_CULL_FACE);
//					for (i = 0; i < atmospheres.GetCount(); i++)
//					{
//						ringsMap->Bind(0);
//
//						ringsTransformation *= RING_ROTATION_CORRECTION;
//
//						ringsProgram->UploadMatrixF("u_transformMatrix", ringsTransformation);
//						ringsProgram->UploadMatrixF("u_viewProjMatrix", projectionMatrix * cam.GetViewMatrix());
//						ringsProgram->UploadFloat("lightPosition", sunPosition);
//
//						glDrawElements(GL_TRIANGLES, ringsVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
//
//						ringsMap->Unbind();
//					}
//					glEnable(GL_CULL_FACE);
//
//					ringsVAO->Unbind();
//					ringsProgram->Unbind();
//
//				}
//				//
//#pragma endregion



}