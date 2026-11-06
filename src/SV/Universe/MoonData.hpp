#pragma once
#include <array>

namespace SV::Universe
{
	// Mean orbital elements of a moon about its planet: a precessing ellipse, fitted to JPL Horizons
	// positions from 2017 to 2035 (see Tools/Moons/make_moon_data.py). Angles are in degrees and
	// rates in degrees per day, at MOON_ELEMENT_EPOCH. The elements are measured in the moon's
	// reference plane (its local Laplace plane, its planet's equator or the ecliptic), given by that
	// plane's pole: the node is measured from the plane's ascending node on the ICRF equator.
	struct MoonData
	{
		const char* name;
		const char* planet;
		int naifId;
		double radiusKm;      // Mean radius
		bool radiusEstimated; // From the moon's brightness, or typical of its group
		double massKg;
		double albedo;        // Geometric
		bool synchronous;     // Keeps one face to its planet
		bool regular;         // A regular moon (otherwise an irregular, captured moon)

		double poleRightAscension, poleDeclination; // Reference plane's pole (ICRF)
		double semiMajorAxisKm;
		double eccentricity;
		double inclination;
		double meanLongitude, meanLongitudeRate;
		double argumentOfPeriapsis, argumentOfPeriapsisRate;
		double ascendingNode, ascendingNodeRate;
		// The fit's worst error between 2017 and 2035 (degrees as seen from the planet), or -1 if
		// Horizons had no positions to fit
		double fitError;
	};

	// The elements' epoch: 2026-01-01 00:00 TDB, in days since J2000.0
	constexpr double MOON_ELEMENT_EPOCH = 9496.5;

	// Every moon of the planets JPL lists, ordered by planet then distance from it
	extern const std::array<MoonData, 460> MOON_DATA;
}
