#include "Planetary System.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Dahlia/Maths/Maths.h"

namespace SV::Universe
{
	PlanetarySystem::PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetNewRenderer>& pRIn) :
		sRenderer(sRIn),
		pRenderer(pRIn)
	{
		if (sRIn == nullptr || pRIn == nullptr)
			throw std::invalid_argument("For a planetary system to be constructed, a valid StarRenderer and PlanetRenderer must be passed.");
	}

	double PlanetarySystem::CircularOrbitSpeed(const double centralMass, const double distance)
	{
		return std::sqrt(SV::Maths::Gravity::G * centralMass / (distance * METRES_PER_UNIT));
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

	PlanetarySystemI PlanetarySystem::Add(const sRef<SV::CelestrialBody>& body)
	{
		switch (body->GetType())
		{
		case CelestrialBodyType::Planet:
			pRenderer->Add(std::static_pointer_cast<SV::PlanetNew>(body));
			break;
		case CelestrialBodyType::Star:
			sRenderer->Add(std::static_pointer_cast<SV::Star>(body));
			break;
		default:
			throw std::runtime_error("CelestrialBodyType is invalid");
		}

		celestrialBodies.push_back(body);
		return celestrialBodies.size() - 1;
	}

	void PlanetarySystem::Remove(const PlanetarySystemI index)
	{
		Remove(sRef<SV::CelestrialBody>(celestrialBodies.at(index)));
	}

	void PlanetarySystem::Remove(const sRef<SV::CelestrialBody>& body)
	{
		switch (body->GetType())
		{
		case CelestrialBodyType::Planet:
			pRenderer->Remove(std::static_pointer_cast<SV::PlanetNew>(body));
			break;
		case CelestrialBodyType::Star:
			sRenderer->Remove(std::static_pointer_cast<SV::Star>(body));
			break;
		default:
			throw std::runtime_error("CelestrialBodyType is invalid");
		}

		std::erase(celestrialBodies, body);
	}

	void PlanetarySystem::Update(double deltaTime)
	{
		using namespace SV::Maths::Gravity;

		for (const auto& body1 : celestrialBodies)
		{
			for (const auto& body2 : celestrialBodies)
			{
				if (body1 == body2) continue;

				const glm::dvec3 separation = METRES_PER_UNIT * ((glm::dvec3)body2->translation - (glm::dvec3)body1->translation);
				const double sqrDst = glm::length2(separation);
				const glm::dvec3 force = glm::normalize(separation) * G * body1->mass * body2->mass / sqrDst;
				const glm::dvec3 acceleration = force / body1->mass;

				body1->velocity += acceleration * deltaTime;
			}
		}

		// Semi-implicit Euler (positions move with the updated velocities), which keeps orbits stable
		for (const auto& body : celestrialBodies)
			body->translation += (glm::vec3)(body->velocity / METRES_PER_UNIT * deltaTime);
	}
}
