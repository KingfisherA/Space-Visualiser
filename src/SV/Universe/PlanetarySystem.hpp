#pragma once
#include <vector>

#include "CelestrialBody.hpp"
#include "Moon.hpp"
#include "Planet.hpp"
#include "Star.hpp"

namespace SV::Universe
{
	// Length of one unit of world space: Earth's mean radius. Velocities are in m/s.
	constexpr double METRES_PER_UNIT = 6371000.0;
	constexpr double KM_PER_UNIT = METRES_PER_UNIT / 1000.0;

	// A set of stars and planets under mutual gravitation, and the planets' moons. Bodies added here
	// are also added to the renderer for their type.
	//
	// The stars and planets are integrated (each planet with its moons as one body at their
	// barycentre). The moons follow orbits computed from their mean elements about their planets,
	// which stay accurate at any time scale (a step of an hour can't follow Phobos' 7.7 hour orbit),
	// and the planets are offset from their systems' barycentres by their moons' pull.
	class PlanetarySystem
	{
	public:
		PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetRenderer>& pRIn);

		void Add(const sRef<SV::CelestrialBody>& body);
		// Adds a moon of `planet` (which must have been added), and its mass to the planet's system
		void AddMoon(const sRef<SV::Moon>& moon, const sRef<SV::CelestrialBody>& planet);

		// Every body, moons included
		[[nodiscard]] const std::vector<sRef<SV::CelestrialBody>>& GetBodies() const { return celestrialBodies; }

		// The simulation's time, in days since J2000.0 (UTC). Setting it moves the moons, not the
		// integrated bodies.
		void SetTime(double daysSinceJ2000);
		[[nodiscard]] double GetTime() const noexcept { return days; }

		// Sets `body`'s velocity so the system's total momentum is zero, keeping its centre of mass
		// still (otherwise the whole system drifts in the direction of the planets' momentum)
		void ZeroMomentum(const sRef<SV::CelestrialBody>& body);

		// Advances the simulation by deltaTime seconds
		void Update(double deltaTime);

		// Places each body's render translation at its position relative to `origin`
		void SetRenderOrigin(const glm::dvec3& origin);

	private:
		// Places the moons for the current time, and their planets around their barycentres
		void UpdateMoons();

		struct MoonOrbit
		{
			sRef<SV::Moon> moon;
			sRef<SV::CelestrialBody> planet;
		};

		std::vector<sRef<SV::CelestrialBody>> celestrialBodies;
		// The integrated bodies (stars, and planets with their moons)
		std::vector<sRef<SV::CelestrialBody>> gravitatingBodies;
		std::vector<MoonOrbit> moons;
		double days = 0.0;
		sRef<SV::StarRenderer> sRenderer;
		sRef<SV::PlanetRenderer> pRenderer;
	};
}
