#include "Atmosphere.hpp"

#include <array>

namespace SV::Atmosphere
{
#pragma region Earth Atmosphere
	//
	EarthAtmosphere::EarthAtmosphere()
	{
		ScatteringData scatData;

		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
		// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
		// summed and averaged in each bin (e.g. the value for 360nm is the average
		// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
		// Values in W.m^-2.
		std::array<double, 48> solarIrradianceValues = {
			1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
				1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
				1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
				1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
				1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
				1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
		};
		scatData.solarIrradiance = solarIrradianceValues;

		std::array<double, 48> ozoneCrossSection = {
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
		constexpr double kMaxOzoneNumberDensity = (300.0 * kDobsonUnit / 15000.0) * 0.1;

		for (double& value : ozoneCrossSection)
		{
			value *= kMaxOzoneNumberDensity;
		}

		// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
		// referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
		// each bin (e.g. the value for 360nm is the average of the original values
		// for all wavelengths between 360 and 370nm). Values in m^2.
		scatData.rayleighExtinction = ozoneCrossSection;

		scatData.kBottomRadius = 6360000.0;
		scatData.kTopRadius = scatData.kBottomRadius + 60000.0;

		constexpr double kRayleighScaleHeight = 8000.0; // Rayleigh layer height for density calculation
		constexpr double kIOR = 1.0002973; //Refractive index of atmospheric gases at the upper atmosphere (nitrogen at 0.5876 micrometre wavelength at 0 celcius)
		scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(kIOR);

		scatData.kMieScaleHeight = 1200.0; //Mie layer height for density calculation
		scatData.kMieAngstromAlpha = 0.0;
		scatData.kMieAngstromBeta = 5.328e-3;
		scatData.kMieSingleScatteringAlbedo = 0.9;
		scatData.kMiePhaseFunctionG = 0.73;

		scatData.kGroundAlbedo = 0.1;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);
		// Density profile increasing linearly from 0 to 1 between 10 and 25km, and
		// decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
		// profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
		// Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
		//Specifies the density function for where the extinction coefficient will be applied to
		//In this case, the extinction coefficients are held within the "kOzoneCrossSection" array for
		//extinction caused by ozone gas.
		scatData.absorptionDensity[0] = DensityProfileLayer(25000.0, 0.0, 0.0, 1.0 / 15000.0, -2.0 / 3.0);
		scatData.absorptionDensity[1] = DensityProfileLayer(0.0, 0.0, 0.0, -1.0 / 15000.0, 8.0 / 3.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);

		surfaceAtmoIntensity = 0.85f;
	}
	//
#pragma endregion
		//
#pragma region Jupiter Atmosphere
	//
	JupiterAtmosphere::JupiterAtmosphere()
	{
		ScatteringData scatData;

		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		std::array<double, 1> kSolarIrradiance{ 1.5 };
		scatData.solarIrradiance = kSolarIrradiance;

		std::array<double, 1> kRayleighExtinctionCoefficient{ 8.5e-8 };
		scatData.rayleighExtinction = kRayleighExtinctionCoefficient;

		scatData.kBottomRadius = 69911000.0;
		scatData.kTopRadius = scatData.kBottomRadius + 5000000.0;

		constexpr double kRayleighScaleHeight = 30000.0; // Rayleigh layer height for density calculation
		constexpr double kIOR = 1.00014930; //Refractive index of atmospheric gases at the upper atmosphere (nitrogen at 0.5876 micrometre wavelength at 0 celcius)
		scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(kIOR);

		scatData.kMieScaleHeight = 1200.0; //Mie layer height for density calculation
		scatData.kMieAngstromAlpha = 0.0;
		scatData.kMieAngstromBeta = 5.328e-3;
		scatData.kMieSingleScatteringAlbedo = 0.9;
		scatData.kMiePhaseFunctionG = 0.73;

		scatData.kGroundAlbedo = 1.5;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);
		scatData.absorptionDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / ((scatData.kTopRadius - scatData.kBottomRadius) * 0.1), 0.0, 0.0);
		scatData.absorptionDensity[1] = DensityProfileLayer();

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);

		surfaceAtmoIntensity = 0.08f;
	}
	//
#pragma endregion
		//
#pragma region Saturn Atmosphere
	//
	SaturnAtmosphere::SaturnAtmosphere()
	{
		ScatteringData scatData;

		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		// Hydrogen (96%) and helium above the 1 bar level, over a cloud deck of ammonia ice veiled by
		// a photochemical haze. The bottom of the atmosphere is the planet's 1 bar surface, which the
		// sky mesh is shaped to (an oblate spheroid; see SV::Saturn).

		// Sunlight at Saturn is 1/90 of Earth's. The scene shares one exposure, so it is dimmed only
		// to 1/5 of Earth's here: enough to keep Saturn's bright clouds (and rings) in the same range
		// as Earth (whose ground reflects much less).
		std::array<double, 1> kSolarIrradiance{ 0.3 };
		scatData.solarIrradiance = kSolarIrradiance;

		// No absorption: methane only absorbs strongly in the near infrared
		std::array<double, 1> kAbsorptionExtinction{ 0.0 };
		scatData.rayleighExtinction = kAbsorptionExtinction;

		scatData.kBottomRadius = 60268000.0;                    // Equatorial radius at 1 bar
		scatData.kTopRadius = scatData.kBottomRadius + 600000.0; // About ten scale heights

		// Scale height at 1 bar (134 K, 2.07 g/mol)
		constexpr double kRayleighScaleHeight = 59500.0;
		// CalculateRayleighCoefficient leaves out the number density, so this is an effective index:
		// H2/He's refractivity (1.28e-4 at STP, 2.6e-4 at 1 bar and 134 K) scaled so the scattering
		// coefficient matches 8 pi^3 (n^2 - 1)^2 / (3 N lambda^4) relative to Earth's, given Saturn's
		// number density at 1 bar (5.4e25 m^-3, twice Earth's at sea level): n = 1.000183, a Rayleigh
		// optical depth of 0.25 at 550 nm. Only the gas above the haze (about the top fifth of the
		// column) is modelled, as the diffuse map already shows the colour seen through the rest:
		// (n - 1) is scaled by sqrt(0.2).
		constexpr double kIOR = 1.0000818;
		scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(kIOR);

		// Upper tropospheric haze: submicron, bright and forward scattering. The diffuse map already
		// shows the haze over the clouds, so only a thin layer is modelled (it gives the limb its glow).
		scatData.kMieScaleHeight = 60000.0;
		scatData.kMieAngstromAlpha = 0.5;
		scatData.kMieAngstromBeta = 0.15; // Vertical optical depth at 1 micrometre
		scatData.kMieSingleScatteringAlbedo = 0.95;
		scatData.kMiePhaseFunctionG = 0.65;

		// The cloud deck seen from the atmosphere (scaled by lambda^-4 in ProcessAtmosphericData)
		scatData.kGroundAlbedo = 0.03;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);

		surfaceAtmoIntensity = 0.1f;
	}
	//
#pragma endregion
//
#pragma region Titan Atmosphere
	//
	TitanAtmosphere::TitanAtmosphere()
	{
		ScatteringData scatData;

		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		std::array<double, 1> kSolarIrradiance{ 1.5 };
		scatData.solarIrradiance = kSolarIrradiance;

		std::array<double, 1>kRayleighExtinctionCoefficient{ 1.5e-6 };
		scatData.rayleighExtinction = kRayleighExtinctionCoefficient;

		scatData.kBottomRadius = 2574700.0;
		scatData.kTopRadius = scatData.kBottomRadius + 595457.0;

		constexpr double kRayleighScaleHeight = 30000.0; // Rayleigh layer height for density calculation
		constexpr double kIOR = 1.00029433; //Refractive index of atmospheric gases at the upper atmosphere (nitrogen at 0.5876 micrometre wavelength at 0 celcius)
		scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(kIOR);

		scatData.kMieScaleHeight = 1200.0; //Mie layer height for density calculation
		scatData.kMieAngstromAlpha = 0.0;
		scatData.kMieAngstromBeta = 5.328e-3;
		scatData.kMieSingleScatteringAlbedo = 0.9;
		scatData.kMiePhaseFunctionG = 0.73;

		scatData.kGroundAlbedo = 1.5;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);

		scatData.absorptionDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / ((scatData.kTopRadius - scatData.kBottomRadius) * 0.1), 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);

		surfaceAtmoIntensity = 0.9f;
	}
	//
#pragma endregion
//
}
