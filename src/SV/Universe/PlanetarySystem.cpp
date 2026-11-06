#include "PlanetarySystem.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

#include "Dahlia/Maths/Maths.h"
#include "MoonData.hpp"

namespace SV::Universe
{
	namespace
	{
		constexpr double SECONDS_PER_DAY = 86400.0;
	}

	PlanetarySystem::PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetRenderer>& pRIn) :
		sRenderer(sRIn),
		pRenderer(pRIn)
	{
		if (sRIn == nullptr || pRIn == nullptr)
			throw std::invalid_argument("For a planetary system to be constructed, a valid StarRenderer and PlanetRenderer must be passed.");
	}

	void PlanetarySystem::ZeroMomentum(const sRef<SV::CelestrialBody>& body)
	{
		glm::dvec3 momentum(0.0);
		for (const auto& other : gravitatingBodies)
		{
			if (other != body)
				momentum += other->systemMass * other->velocity;
		}
		body->velocity = -momentum / body->systemMass;
	}

	void PlanetarySystem::Add(const sRef<SV::CelestrialBody>& body)
	{
		switch (body->GetType())
		{
		case CelestrialBodyType::Planet:
			pRenderer->Add(std::static_pointer_cast<SV::Planet>(body));
			break;
		case CelestrialBodyType::Star:
			sRenderer->Add(std::static_pointer_cast<SV::Star>(body));
			break;
		case CelestrialBodyType::Moon:
			throw std::invalid_argument("Moons are added with AddMoon");
		default:
			throw std::runtime_error("CelestrialBodyType is invalid");
		}

		celestrialBodies.push_back(body);
		gravitatingBodies.push_back(body);
	}

	void PlanetarySystem::AddMoon(const sRef<SV::Moon>& moon, const sRef<SV::CelestrialBody>& planet)
	{
		if (std::find(gravitatingBodies.begin(), gravitatingBodies.end(), planet) == gravitatingBodies.end())
			throw std::invalid_argument("A moon's planet must be added to the system first");

		pRenderer->Add(moon);
		celestrialBodies.push_back(moon);
		moons.push_back({ moon, planet });
		planet->systemMass += moon->GetMass();
		UpdateMoons();
	}

	void PlanetarySystem::SetTime(const double daysSinceJ2000)
	{
		days = daysSinceJ2000;
		UpdateMoons();
	}

	void PlanetarySystem::UpdateMoons()
	{
		// Each moon's offset from its planet, and the moons' pull on their planets (mass-weighted
		// offsets, which put the planets' systems' barycentres where the integration has them)
		std::unordered_map<CelestrialBody*, glm::dvec3> pull;
		for (const MoonOrbit& orbit : moons)
		{
			const glm::dvec3 offset = orbit.moon->GetOrbitOffset(days);
			// The orbit's pole, from its motion over a 200th of a turn
			const double step = 360.0 / std::max(std::abs(orbit.moon->GetData().meanLongitudeRate), 1e-6) / 200.0;
			const glm::dvec3 pole = glm::cross(offset, orbit.moon->GetOrbitOffset(days + step));
			orbit.moon->Orient(offset, glm::normalize(pole));
			orbit.moon->position = offset; // Relative to the planet for now
			pull[orbit.planet.get()] += orbit.moon->GetMass() * offset;
		}

		for (const auto& [planet, moment] : pull)
		{
			const glm::dvec3 barycentre = planet->position - planet->barycentreOffset;
			planet->barycentreOffset = -moment / planet->systemMass;
			planet->position = barycentre + planet->barycentreOffset;
		}
		for (const MoonOrbit& orbit : moons)
			orbit.moon->position += orbit.planet->position;
	}

	void PlanetarySystem::Update(const double deltaTime)
	{
		using namespace SV::Maths::Gravity;

		// Steps of at most an hour: Mercury's 88 day orbit then takes over 2000 steps
		constexpr double MAX_STEP = 3600.0;
		const int steps = std::max(1, (int)std::ceil(deltaTime / MAX_STEP));
		const double step = deltaTime / steps;

		for (int i = 0; i < steps; i++)
		{
			// Between the systems' barycentres (the offsets cancel, as each moves with its barycentre
			// within a step)
			for (const auto& body1 : gravitatingBodies)
			{
				for (const auto& body2 : gravitatingBodies)
				{
					if (body1 == body2) continue;

					const glm::dvec3 separation = METRES_PER_UNIT * ((body2->position - body2->barycentreOffset) -
																	 (body1->position - body1->barycentreOffset));
					const double sqrDst = glm::length2(separation);
					const glm::dvec3 acceleration = glm::normalize(separation) * G * body2->systemMass / sqrDst;
					body1->velocity += acceleration * step;
				}
			}

			// Semi-implicit Euler (positions move with the updated velocities), which keeps orbits stable
			for (const auto& body : gravitatingBodies)
				body->position += body->velocity / METRES_PER_UNIT * step;
		}

		days += deltaTime / SECONDS_PER_DAY;
		UpdateMoons();
	}

	void PlanetarySystem::SetRenderOrigin(const glm::dvec3& origin)
	{
		for (const auto& body : celestrialBodies)
			body->Translate(glm::vec3(body->position - origin));
	}
}
