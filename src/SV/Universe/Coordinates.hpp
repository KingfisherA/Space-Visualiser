#pragma once
#include <cmath>

#include <glm/glm.hpp>

namespace SV::Universe
{
	constexpr double DEGREES = 3.14159265358979323846 / 180.0;
	// Obliquity of the ecliptic at J2000.0 (IAU 1976)
	constexpr double OBLIQUITY = 84381.448 / 3600.0 * DEGREES;
	// Terrestrial Time - UTC since the 2017 leap second: the orbits are computed in TT (TDB)
	constexpr double TT_MINUS_UTC_DAYS = 69.184 / 86400.0;

	// Ecliptic J2000 coordinates (x to the vernal equinox, z to ecliptic north) to world coordinates
	// (the ecliptic is the world's XZ plane, ecliptic north +Y)
	inline glm::dvec3 EclipticToWorld(const glm::dvec3& v)
	{
		return { v.x, v.z, -v.y };
	}

	// Equatorial (ICRF) coordinates to ecliptic J2000
	inline glm::dvec3 EquatorialToEcliptic(const glm::dvec3& v)
	{
		return { v.x, std::cos(OBLIQUITY) * v.y + std::sin(OBLIQUITY) * v.z,
				 -std::sin(OBLIQUITY) * v.y + std::cos(OBLIQUITY) * v.z };
	}

	// A direction given by right ascension and declination (degrees, ICRF) in ecliptic coordinates
	inline glm::dvec3 EquatorialDirectionToEcliptic(const double rightAscension, const double declination)
	{
		const double ra = rightAscension * DEGREES, dec = declination * DEGREES;
		return EquatorialToEcliptic({ std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec) });
	}

	// A direction given by right ascension and declination (degrees, ICRF) in world coordinates
	inline glm::dvec3 EquatorialDirectionToWorld(const double rightAscension, const double declination)
	{
		return EclipticToWorld(EquatorialDirectionToEcliptic(rightAscension, declination));
	}
}
