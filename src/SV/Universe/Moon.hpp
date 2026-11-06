#pragma once
#include <string>

#include <glm/glm.hpp>

#include "Planet.hpp"

namespace SV
{
	namespace Universe
	{
		struct MoonData;
	}

	// A moon: drawn like a planet (an airless moon has a vacuum for an atmosphere), on an orbit
	// computed from its mean elements about its planet rather than integrated. Synchronous moons
	// keep their map's prime meridian (longitude 0) turned to the planet.
	class Moon : public Planet
	{
	public:
		Moon(const Universe::MoonData& data, const sRef<SV::Atmosphere::IAtmosphere>& atmosphere);

		[[nodiscard]] const Universe::MoonData& GetData() const noexcept { return data; }
		[[nodiscard]] const std::string& GetName() const noexcept { return name; }

		// Position relative to the planet (world units, world axes) at a time in days since J2000.0
		// (UTC)
		[[nodiscard]] glm::dvec3 GetOrbitOffset(double days) const;
		// Turns the moon for its offset from its planet: its axis along its orbit's pole (the one on
		// the north side of the ecliptic), and if it is synchronous, its prime meridian to the planet
		void Orient(const glm::dvec3& offset, const glm::dvec3& orbitPole);

	private:
		const Universe::MoonData& data;
		std::string name;
		// The reference plane's axes (its node on the ICRF equator, 90 degrees on, and its pole) in
		// ecliptic coordinates
		glm::dmat3 referenceFrame{ 1.0 };
		bool hasOrientation = false;
	};
}
