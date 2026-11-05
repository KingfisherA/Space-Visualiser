#pragma once
#include <vector>

#include "CelestrialBody.hpp"
#include "PlanetNew.hpp"
#include "Star.hpp"

namespace SV::Universe
{
	using PlanetarySystemI = size_t;

	// Length of one unit of world space in the gravity simulation. Velocities are in m/s.
	constexpr double METRES_PER_UNIT = 1E9;

	// A set of stars and planets under mutual gravitation. Bodies added here are also added to the
	// renderer for their type.
	class PlanetarySystem
	{
	public:
		// Speed (m/s) of a circular orbit `distance` world units from a body of mass `centralMass` (kg)
		[[nodiscard]] static double CircularOrbitSpeed(double centralMass, double distance);

		PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetNewRenderer>& pRIn);

		PlanetarySystemI Add(const sRef<SV::CelestrialBody>& body);
		void Remove(PlanetarySystemI index);
		void Remove(const sRef<SV::CelestrialBody>& body);

		[[nodiscard]] const sRef<SV::CelestrialBody>& Get(PlanetarySystemI index) const { return celestrialBodies.at(index); }
		[[nodiscard]] const std::vector<sRef<SV::CelestrialBody>>& GetBodies() const { return celestrialBodies; }

		// Sets `body`'s velocity so the system's total momentum is zero, keeping its centre of mass
		// still (otherwise the whole system drifts in the direction of the planets' momentum)
		void ZeroMomentum(const sRef<SV::CelestrialBody>& body);

		// Advances the simulation by deltaTime seconds
		void Update(double deltaTime);

	private:
		std::vector<sRef<SV::CelestrialBody>> celestrialBodies;
		sRef<SV::StarRenderer> sRenderer;
		sRef<SV::PlanetNewRenderer> pRenderer;
	};
}
