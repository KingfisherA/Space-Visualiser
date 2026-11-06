#include "Atmosphere.hpp"

#include <array>
#include <cmath>

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
#pragma region Shared
	//
	namespace
	{
		// Mean solar spectral irradiance at Earth in the visible (W m^-2 nm^-1), roughly the mean of
		// Earth's ASTM G-173 spectrum
		constexpr double EARTH_SOLAR_IRRADIANCE = 1.5;

		// Sunlight on a planet `distance` AU from the Sun. The scene shares one exposure, and with the
		// inverse square law Neptune (900 times dimmer than Earth) would be black: the irradiance falls
		// as (1 / d^2)^0.35 instead, which gives Saturn a fifth of Earth's rather than a ninetieth.
		double DisplaySolarIrradiance(const double distance)
		{
			return EARTH_SOLAR_IRRADIANCE * std::pow(distance, -0.7);
		}

		// CalculateRayleighCoefficient leaves out the number density N (it assumes Earth's at sea level,
		// N0), so it takes an effective index: matching 8 pi^3 (n^2 - 1)^2 / (3 N lambda^4) for a gas
		// with refractivity `stpRefractivity` (n - 1 at 0 C and 1 atm) at `pressure` (bar) and
		// `temperature` (K) needs (n - 1) = stpRefractivity * sqrt(N / N0).
		double EffectiveIndex(const double stpRefractivity, const double pressure, const double temperature)
		{
			return 1.0 + stpRefractivity * std::sqrt(pressure * 273.15 / temperature);
		}

		// A giant planet: gas over a cloud deck veiled by haze. Only the gas above the haze (about the
		// top fifth of the column above 1 bar) is modelled, as the diffuse map already shows the colour
		// seen through the rest: (n - 1) is scaled by sqrt(0.2). The haze is submicron, bright and
		// strongly forward scattering; only a thin layer is modelled, which gives the limb its glow (it
		// rings the night side when the planet is seen against the Sun).
		struct GiantPlanetParameters
		{
			double equatorialRadius;  // m, at 1 bar
			double distance;          // AU from the Sun
			double stpRefractivity;   // of the gas mixture
			double temperature;       // K, at 1 bar
			double gasScaleHeight;    // m, at 1 bar
			double hazeScaleHeight;   // m
			double hazeOpticalDepth;  // Vertical, at 1 micrometre
			double hazePhaseG;
			double atmosphereHeight;  // m, above 1 bar
		};

		sRef<AtmospherePrecomputer> CreateGiantPlanetAtmosphere(const GiantPlanetParameters& planet)
		{
			ScatteringData scatData;
			scatData.kLambdaMin = 360;
			scatData.kLambdaMax = 830;

			const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(planet.distance) };
			scatData.solarIrradiance = solarIrradiance;

			// No absorption: methane only absorbs strongly in the red and near infrared, which the
			// diffuse maps already show
			const std::array<double, 1> absorptionExtinction{ 0.0 };
			scatData.rayleighExtinction = absorptionExtinction;

			scatData.kBottomRadius = planet.equatorialRadius;
			scatData.kTopRadius = planet.equatorialRadius + planet.atmosphereHeight;

			const double gasIndex = EffectiveIndex(planet.stpRefractivity * std::sqrt(0.2), 1.0, planet.temperature);
			scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(gasIndex);

			scatData.kMieScaleHeight = planet.hazeScaleHeight;
			scatData.kMieAngstromAlpha = 0.5;
			scatData.kMieAngstromBeta = planet.hazeOpticalDepth;
			scatData.kMieSingleScatteringAlbedo = 0.95;
			scatData.kMiePhaseFunctionG = planet.hazePhaseG;

			// The cloud deck seen from the atmosphere (scaled by lambda^-4 in ProcessAtmosphericData)
			scatData.kGroundAlbedo = 0.03;

			scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / planet.gasScaleHeight, 0.0, 0.0);
			scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / planet.hazeScaleHeight, 0.0, 0.0);

			return make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
		}

		// A vacuum: vanishingly thin, but not zero (the precomputation divides by the scattering
		// coefficients)
		sRef<AtmospherePrecomputer> CreateVacuum(const double radius, const double height, const double distance)
		{
			ScatteringData scatData;
			scatData.kLambdaMin = 360;
			scatData.kLambdaMax = 830;

			const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(distance) };
			scatData.solarIrradiance = solarIrradiance;
			const std::array<double, 1> absorptionExtinction{ 0.0 };
			scatData.rayleighExtinction = absorptionExtinction;

			scatData.kBottomRadius = radius;
			scatData.kTopRadius = radius + height;

			constexpr double kScaleHeight = 10000.0;
			scatData.kRayleighScatteringCoefficient = AtmospherePrecomputer::CalculateRayleighCoefficient(1.0 + 1e-8);
			scatData.kMieScaleHeight = kScaleHeight;
			scatData.kMieAngstromAlpha = 0.0;
			scatData.kMieAngstromBeta = 1e-9;
			scatData.kMieSingleScatteringAlbedo = 0.9;
			scatData.kMiePhaseFunctionG = 0.7;
			scatData.kGroundAlbedo = 0.01;

			scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);
			scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);

			return make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
		}
	}
	//
#pragma endregion
	//
#pragma region Terrestrial Atmospheres
	//
	MercuryAtmosphere::MercuryAtmosphere()
	{
		atmoPrecomputer = CreateVacuum(2439700.0, 50000.0, 0.387);
	}

	AirlessAtmosphere::AirlessAtmosphere(const double distance)
	{
		// A shell 1% of the body's radius thick (the sky mesh is drawn at its top)
		atmoPrecomputer = CreateVacuum(1000000.0, 10000.0, distance);
	}

	VenusAtmosphere::VenusAtmosphere()
	{
		ScatteringData scatData;
		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(0.723) };
		scatData.solarIrradiance = solarIrradiance;
		const std::array<double, 1> absorptionExtinction{ 0.0 };
		scatData.rayleighExtinction = absorptionExtinction;

		// The bottom is the top of the sulphuric acid clouds, about 70 km up, where the pressure is
		// about 40 mbar: the clouds hide everything below
		scatData.kBottomRadius = 6051800.0;
		scatData.kTopRadius = scatData.kBottomRadius + 80000.0;

		// Carbon dioxide (refractivity 4.49e-4) at 40 mbar and 230 K
		constexpr double kRayleighScaleHeight = 4900.0;
		scatData.kRayleighScatteringCoefficient =
			AtmospherePrecomputer::CalculateRayleighCoefficient(EffectiveIndex(4.49e-4, 0.04, 230.0));

		// The upper haze over the clouds: submicron sulphuric acid droplets, which make the limb glow
		scatData.kMieScaleHeight = 5000.0;
		scatData.kMieAngstromAlpha = 1.0;
		scatData.kMieAngstromBeta = 0.15;
		scatData.kMieSingleScatteringAlbedo = 0.995;
		scatData.kMiePhaseFunctionG = 0.75;

		// The bright clouds (scaled by lambda^-4 in ProcessAtmosphericData)
		scatData.kGroundAlbedo = 0.07;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
	}

	MarsAtmosphere::MarsAtmosphere()
	{
		ScatteringData scatData;
		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(1.524) };
		scatData.solarIrradiance = solarIrradiance;

		scatData.kBottomRadius = 3396200.0;
		scatData.kTopRadius = scatData.kBottomRadius + 100000.0;

		// Carbon dioxide at 6 mbar and 210 K: a Rayleigh optical depth of only 0.004
		constexpr double kScaleHeight = 11100.0;
		scatData.kRayleighScatteringCoefficient =
			AtmospherePrecomputer::CalculateRayleighCoefficient(EffectiveIndex(4.49e-4, 0.006, 210.0));

		// Suspended dust (a few micrometres across, so grey extinction) mixed through the lower
		// atmosphere: an optical depth of about 0.4 outside dust storms
		scatData.kMieScaleHeight = kScaleHeight;
		scatData.kMieAngstromAlpha = 0.0;
		scatData.kMieAngstromBeta = 0.4;
		scatData.kMieSingleScatteringAlbedo = 0.97;
		scatData.kMiePhaseFunctionG = 0.65;

		// The dust's iron oxides absorb blue light, which gives the butterscotch sky. The absorption
		// follows the dust; ProcessAtmosphericData scales it by lambda^-4 (in micrometres), which gives
		// an albedo of about 0.75 at 450 nm and 0.95 at 650 nm.
		const std::array<double, 1> dustAbsorption{ 3.5e-7 };
		scatData.rayleighExtinction = dustAbsorption;
		scatData.absorptionDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);

		// Scaled by lambda^-4 in ProcessAtmosphericData
		scatData.kGroundAlbedo = 0.02;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
	}
	//
#pragma endregion
	//
#pragma region Moon Atmospheres
	//
	TitanAtmosphere::TitanAtmosphere()
	{
		ScatteringData scatData;
		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(9.537) };
		scatData.solarIrradiance = solarIrradiance;

		// The haze reaches about 500 km (a detached layer at its top gives Cassini's views of the limb
		// their blue rim)
		scatData.kBottomRadius = 2574700.0;
		scatData.kTopRadius = scatData.kBottomRadius + 600000.0;

		// Nitrogen at 1.47 bar and 94 K on the surface: with Titan's low gravity there is ten times
		// Earth's column of gas (a Rayleigh optical depth of about 1 at 550 nm). Scale height 21 km
		// (28 g/mol, g = 1.35 m s^-2).
		constexpr double kRayleighScaleHeight = 21000.0;
		scatData.kRayleighScatteringCoefficient =
			AtmospherePrecomputer::CalculateRayleighCoefficient(EffectiveIndex(2.98e-4, 1.47, 94.0));

		// The haze (fractal aggregates of tholins): an optical depth of about 4 at 550 nm, falling
		// with wavelength, forward scattering (Huygens DISR, Tomasko et al. 2008)
		constexpr double kHazeScaleHeight = 65000.0;
		scatData.kMieScaleHeight = kHazeScaleHeight;
		scatData.kMieAngstromAlpha = 1.2;
		scatData.kMieAngstromBeta = 1.95;
		scatData.kMieSingleScatteringAlbedo = 0.995;
		scatData.kMiePhaseFunctionG = 0.65;

		// The tholins absorb blue light: absorption with the haze's profile, scaled by lambda^-4 (in
		// micrometres) in ProcessAtmosphericData, which gives the haze a single scattering albedo of
		// about 0.67 at 440 nm, 0.80 at 550 nm and 0.88 at 680 nm. Many scatterings in the deep haze
		// compound that into Titan's orange; where the haze thins out at the limb it looks blue.
		const std::array<double, 1> hazeAbsorption{ 1.4e-6 };
		scatData.rayleighExtinction = hazeAbsorption;
		scatData.absorptionDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kHazeScaleHeight, 0.0, 0.0);

		// The surface, dark beneath the haze (scaled by lambda^-4 in ProcessAtmosphericData)
		scatData.kGroundAlbedo = 0.01;

		// From 600 km up the Sun stays in view until 32 degrees past the terminator, and the haze
		// scatters its light further: twilight reaches far round the night side
		scatData.kMaxSunZenithAngle = 145.0;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kRayleighScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kHazeScaleHeight, 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
	}

	TritonAtmosphere::TritonAtmosphere()
	{
		ScatteringData scatData;
		scatData.kLambdaMin = 360;
		scatData.kLambdaMax = 830;

		const std::array<double, 1> solarIrradiance{ DisplaySolarIrradiance(30.07) };
		scatData.solarIrradiance = solarIrradiance;
		const std::array<double, 1> absorptionExtinction{ 0.0 };
		scatData.rayleighExtinction = absorptionExtinction;

		scatData.kBottomRadius = 1353400.0;
		scatData.kTopRadius = scatData.kBottomRadius + 60000.0;

		// Nitrogen at 14 microbar and 38 K; scale height 14 km (g = 0.78 m s^-2)
		constexpr double kScaleHeight = 14000.0;
		scatData.kRayleighScatteringCoefficient =
			AtmospherePrecomputer::CalculateRayleighCoefficient(EffectiveIndex(2.98e-4, 1.4e-5, 38.0));

		// Thin, bright haze layers below about 30 km, with an optical depth of 0.003 to 0.01, seen by
		// Voyager 2 above the limb (Rages and Pollack 1992)
		scatData.kMieScaleHeight = 10000.0;
		scatData.kMieAngstromAlpha = 1.0;
		scatData.kMieAngstromBeta = 0.004;
		scatData.kMieSingleScatteringAlbedo = 0.99;
		scatData.kMiePhaseFunctionG = 0.6;

		// The nitrogen frost (scaled by lambda^-4 in ProcessAtmosphericData)
		scatData.kGroundAlbedo = 0.06;

		scatData.rayleighDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / kScaleHeight, 0.0, 0.0);
		scatData.mieDensity[0] = DensityProfileLayer(0.0, 1.0, -1.0 / scatData.kMieScaleHeight, 0.0, 0.0);

		atmoPrecomputer = make<AtmospherePrecomputer>(scatData, kLengthUnitInMeters);
	}
	//
#pragma endregion
	//
#pragma region Giant Planet Atmospheres
	//
	// Refractivities at 0 C and 1 atm: H2 1.32e-4, He 3.5e-5, CH4 4.4e-4
	JupiterAtmosphere::JupiterAtmosphere()
	{
		// 86% H2, 14% He at 165 K; scale height 27 km (2.22 g/mol, g = 24.8 m s^-2)
		atmoPrecomputer = CreateGiantPlanetAtmosphere({
			.equatorialRadius = 71492000.0,
			.distance = 5.203,
			.stpRefractivity = 1.18e-4,
			.temperature = 165.0,
			.gasScaleHeight = 27000.0,
			.hazeScaleHeight = 60000.0,
			.hazeOpticalDepth = 0.1,
			.hazePhaseG = 0.75,
			.atmosphereHeight = 600000.0 });
	}

	SaturnAtmosphere::SaturnAtmosphere()
	{
		// 96% H2 at 134 K; scale height 59.5 km (2.07 g/mol, g = 10.4 m s^-2). The haze (fractal
		// aggregates; Cassini's high phase views give g of about 0.8) reaches far higher than the
		// gas's scale height, with detached layers hundreds of km up.
		atmoPrecomputer = CreateGiantPlanetAtmosphere({
			.equatorialRadius = 60268000.0,
			.distance = 9.537,
			.stpRefractivity = 1.28e-4,
			.temperature = 134.0,
			.gasScaleHeight = 59500.0,
			.hazeScaleHeight = 150000.0,
			.hazeOpticalDepth = 0.15,
			.hazePhaseG = 0.8,
			.atmosphereHeight = 1000000.0 });
	}

	UranusAtmosphere::UranusAtmosphere()
	{
		// 83% H2, 15% He, 2.3% CH4 at 76 K; scale height 28 km (2.64 g/mol, g = 8.9 m s^-2)
		atmoPrecomputer = CreateGiantPlanetAtmosphere({
			.equatorialRadius = 25559000.0,
			.distance = 19.19,
			.stpRefractivity = 1.25e-4,
			.temperature = 76.0,
			.gasScaleHeight = 28000.0,
			.hazeScaleHeight = 50000.0,
			.hazeOpticalDepth = 0.05,
			.hazePhaseG = 0.7,
			.atmosphereHeight = 500000.0 });
	}

	NeptuneAtmosphere::NeptuneAtmosphere()
	{
		// 80% H2, 19% He, 1.5% CH4 at 72 K; scale height 21 km (2.6 g/mol, g = 11.2 m s^-2)
		atmoPrecomputer = CreateGiantPlanetAtmosphere({
			.equatorialRadius = 24764000.0,
			.distance = 30.07,
			.stpRefractivity = 1.25e-4,
			.temperature = 72.0,
			.gasScaleHeight = 21000.0,
			.hazeScaleHeight = 40000.0,
			.hazeOpticalDepth = 0.05,
			.hazePhaseG = 0.7,
			.atmosphereHeight = 400000.0 });
	}
	//
#pragma endregion
}
