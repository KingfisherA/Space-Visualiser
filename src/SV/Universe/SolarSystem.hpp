#pragma once
#include <string>
#include <vector>

#include "Dahlia/Utilities/Memory.h"
#include "CelestrialBody.hpp"

namespace SV::Universe
{
	class PlanetarySystem;

	// One astronomical unit in world units (Earth radii)
	constexpr double AU_IN_UNITS = 149597870.7 / 6371.0;

	struct SolarSystemBody
	{
		std::string name;
		sRef<CelestrialBody> body;
	};

	// Days since the J2000.0 epoch (2000-01-01 12:00 UTC) now
	double DaysSinceJ2000Now();

	// Adds the Sun and the eight planets to `system`, all to scale: their sizes, their orbits (from
	// JPL's Keplerian elements, at `daysSinceJ2000`) and the directions of their rotation axes.
	// The ecliptic is the world's XZ plane, with ecliptic north up (+Y) and the vernal equinox along
	// +X. Returns the bodies, the Sun first, in order from the Sun.
	std::vector<SolarSystemBody> CreateSolarSystem(PlanetarySystem& system, double daysSinceJ2000);
}
