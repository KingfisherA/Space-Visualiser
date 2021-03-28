#include"Planetary System.h"
#include"SV/Utilities/Errors.hpp"
#include"SV/Utilities/Maths.hpp"

//
//
namespace SV::Universe
{
	//
	PlanetarySystem::PlanetarySystem(const Ref<SV::StarRenderer>& sRIn, Ref<SV::PlanetNewRenderer>& pRIn) :
		sRenderer(sRIn),
		pRenderer(pRIn)
	{
		if (sRIn == nullptr || pRIn == nullptr)
		{
			SV_FATAL_T(SV::PlanetarySystem, "For a planatery system to be constructed, a valid StarRenderer and PlanetRenderer must be passed.");
			throw std::exception("For a planatery system to be constructed, a valid StarRenderer and PlanetRenderer must be passed.");
		}
	}
	//
	const PlanetarySystemI PlanetarySystem::Add(Ref<SV::CelestrialBody> body)
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
			throw std::exception("CelestrialBodyType is invalid");
			break;
		}

		return this->celestrialBodies.Add(body);
	}
	//
	const void PlanetarySystem::Remove(const PlanetarySystemI index)
	{
		const Ref<SV::CelestrialBody>& body = celestrialBodies[index];

		switch (body->GetType())
		{
		case CelestrialBodyType::Planet:
			pRenderer->Remove(std::static_pointer_cast<SV::PlanetNew>(body));
			break;
		case CelestrialBodyType::Star:
			sRenderer->Remove(std::static_pointer_cast<SV::Star>(body));
			break;
		default:
			throw std::exception("CelestrialBodyType is invalid");
			break;
		}

		celestrialBodies.Remove(index);
	}
	//
	const void PlanetarySystem::Remove(const Ref<SV::CelestrialBody>& body)
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
			throw std::exception("CelestrialBodyType is invalid");
			break;
		}

		celestrialBodies.Remove(body);
	}
	//
	PlanetarySystemI PlanetarySystem::Find(const Ref<SV::CelestrialBody>& body) const
	{
		return celestrialBodies.Find(body);
	}
	//
	void PlanetarySystem::Update(double deltaTime)
	{
		using namespace SV::Maths::Gravity;

		size_t i = 0;
		for (i = 0; i < celestrialBodies.GetCount(); i++)
		{
			const auto& body1 = celestrialBodies[i];

			for (size_t j = 0; j < celestrialBodies.GetCount(); j++)
			{
				const auto& body2 = celestrialBodies[j];
				if (body1 == body2) continue;

				//Assume distances 1 unit = 1x10^6 km * 1x10^9
				double sqrDst = glm::length2((1E9 * (glm::dvec3)body2->translation) - (1E9 * (glm::dvec3)body1->translation));
				glm::dvec3 forceDirection = glm::normalize((1E9 * (glm::dvec3)body2->translation) - (1E9 * (glm::dvec3)body1->translation));
				glm::dvec3 force = forceDirection * G * body1->mass * body2->mass / sqrDst;
				glm::dvec3 acceleration = force / body1->mass;

				body1->velocity += acceleration * deltaTime;
			}
		}

		for (i = 0; i < celestrialBodies.GetCount(); i++)
		{
			const auto& body1 = celestrialBodies[i];
			body1->translation += (glm::vec3)(body1->velocity / 1.0E6 * deltaTime);

			//SV::Console::WriteVector(SV::IntegralToString(i) + " Position: ", (glm::vec3)body1->translation);
			//SV::Console::NewLine();
		}
	}
	//
}