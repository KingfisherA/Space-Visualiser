#pragma once
#include <vector>

#include "CelestrialBody.hpp"
#include "Planet.hpp"
#include "Star.hpp"

namespace SV::Universe
{
	// Length of one unit of world space: Earth's mean radius. Velocities are in m/s.
	constexpr double METRES_PER_UNIT = 6371000.0;
	constexpr double KM_PER_UNIT = METRES_PER_UNIT / 1000.0;

	// A set of stars and planets under mutual gravitation. Bodies added here are also added to the
	// renderer for their type.
	class PlanetarySystem
	{
	public:
		PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetRenderer>& pRIn);

		void Add(const sRef<SV::CelestrialBody>& body);
		[[nodiscard]] const std::vector<sRef<SV::CelestrialBody>>& GetBodies() const { return celestrialBodies; }

		// Sets `body`'s velocity so the system's total momentum is zero, keeping its centre of mass
		// still (otherwise the whole system drifts in the direction of the planets' momentum)
		void ZeroMomentum(const sRef<SV::CelestrialBody>& body);

		// Advances the simulation by deltaTime seconds
		void Update(double deltaTime);

		// Places each body's render translation at its position relative to `origin`
		void SetRenderOrigin(const glm::dvec3& origin);

	private:
		std::vector<sRef<SV::CelestrialBody>> celestrialBodies;
		sRef<SV::StarRenderer> sRenderer;
		sRef<SV::PlanetRenderer> pRenderer;
	};
}
