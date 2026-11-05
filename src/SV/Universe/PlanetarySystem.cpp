#include "PlanetarySystem.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Dahlia/Maths/Maths.h"

namespace SV::Universe
{
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
		for (const auto& other : celestrialBodies)
		{
			if (other != body)
				momentum += other->mass * other->velocity;
		}
		body->velocity = -momentum / body->mass;
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
		default:
			throw std::runtime_error("CelestrialBodyType is invalid");
		}

		celestrialBodies.push_back(body);
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
			for (const auto& body1 : celestrialBodies)
			{
				for (const auto& body2 : celestrialBodies)
				{
					if (body1 == body2) continue;

					const glm::dvec3 separation = METRES_PER_UNIT * (body2->position - body1->position);
					const double sqrDst = glm::length2(separation);
					const glm::dvec3 acceleration = glm::normalize(separation) * G * body2->mass / sqrDst;
					body1->velocity += acceleration * step;
				}
			}

			// Semi-implicit Euler (positions move with the updated velocities), which keeps orbits stable
			for (const auto& body : celestrialBodies)
				body->position += body->velocity / METRES_PER_UNIT * step;
		}
	}

	void PlanetarySystem::SetRenderOrigin(const glm::dvec3& origin)
	{
		for (const auto& body : celestrialBodies)
			body->Translate(glm::vec3(body->position - origin));
	}
}
