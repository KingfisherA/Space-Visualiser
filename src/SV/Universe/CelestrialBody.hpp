#pragma once
#include <functional>
#include <limits>
#include <map>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "Dahlia/Utilities/Memory.h"
#include "SV/Scene/Transformation.hpp"

namespace SV
{
	namespace Universe
	{
		class PlanetarySystem;
	}

	enum class CelestrialBodyType
	{
		Star = 1,
		Planet = 2
	};

	// A body of the gravity simulation. Its position is kept in double precision (the solar system
	// spans millions of world units, beyond float precision); its Transformation's translation is
	// its position relative to the render origin (see PlanetarySystem::SetRenderOrigin), which keeps
	// the coordinates the GPU sees small.
	class CelestrialBody : public Transformation
	{
	public:
		CelestrialBody(const CelestrialBodyType typeIn, const double massIn) : mass(massIn), mytype(typeIn) {}

		[[nodiscard]] CelestrialBodyType GetType() const noexcept { return mytype; }
		[[nodiscard]] double GetMass() const noexcept { return mass; }
		// World units (see Universe::METRES_PER_UNIT)
		[[nodiscard]] const glm::dvec3& GetPosition() const noexcept { return position; }

		void SetPosition(const glm::dvec3& pos) { position = pos; }
		void SetVelocity(const glm::dvec3& vel) { velocity = vel; }

	protected:
		friend class SV::Universe::PlanetarySystem;

		glm::dvec3 position = { 0.0, 0.0, 0.0 };
		glm::dvec3 velocity = { 0.0, 0.0, 0.0 };
		double mass;
		CelestrialBodyType mytype;
	};

	template<class CelestrialType>
	class CelestrialRenderer
	{
	public:
		// Multimap, as bodies at equal distances from the camera would otherwise overwrite each other.
		// Ordered furthest first, so iterating it draws back to front.
		using DistanceCache = std::multimap<float, size_t, std::greater<float>>;
		static_assert(std::is_base_of_v<CelestrialBody, CelestrialType>, "CelestrialType must be a derived class of CelestrialBody");

		void Add(const sRef<CelestrialType>& body) { celestrialBodies.push_back(body); }

		void PopulateDistanceCache(const glm::vec3& camPos)
		{
			distanceCache.clear();
			for (size_t i = 0; i < celestrialBodies.size(); i++)
				distanceCache.insert({ glm::distance2(celestrialBodies[i]->GetTranslation(), camPos), i });
		}

		// Squared distance to the closest body, or infinity if there are none
		[[nodiscard]] float GetClosestDistance() const
		{
			return distanceCache.empty() ? std::numeric_limits<float>::infinity() : distanceCache.rbegin()->first;
		}

	protected:
		std::vector<sRef<CelestrialType>> celestrialBodies;
		DistanceCache distanceCache;
	};
}
