#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
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
		Null = -1,
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
		CelestrialBody(const CelestrialBodyType typeIn, const double massIn) : Transformation(), mass(massIn), mytype(typeIn) {}
		CelestrialBody(const CelestrialBodyType typeIn, const double massIn, const glm::dvec3& posIn) :
			Transformation(glm::vec3(posIn)), position(posIn), mass(massIn), mytype(typeIn) {}

		[[nodiscard]] CelestrialBodyType GetType() const noexcept { return mytype; }
		[[nodiscard]] double GetMass() const noexcept { return mass; }
		// World units (see Universe::METRES_PER_UNIT)
		[[nodiscard]] const glm::dvec3& GetPosition() const noexcept { return position; }
		// m/s
		[[nodiscard]] const glm::dvec3& GetVelocity() const noexcept { return velocity; }

		void SetPosition(const glm::dvec3& pos) { position = pos; }
		void SetVelocity(const glm::dvec3& vel) { velocity = vel; }

	protected:
		friend class SV::Universe::PlanetarySystem;

		glm::dvec3 position = { 0.0, 0.0, 0.0 };
		glm::dvec3 velocity = { 0.0, 0.0, 0.0 };
		double mass;
		CelestrialBodyType mytype;
	};

	using CelestrialIndex = size_t;

	template<class CelestrialType>
	class CelestrialRenderer
	{
	public:
		// Multimap, as bodies at equal distances from the camera would otherwise overwrite each other.
		// Ordered furthest first, so iterating it draws back to front.
		using DistanceCache = std::multimap<float, size_t, std::greater<float>>;
		static_assert(std::is_base_of_v<CelestrialBody, CelestrialType>, "CelestrialType must be a derived class of CelestrialBody");

		CelestrialIndex Add(const sRef<CelestrialType>& body)
		{
			celestrialBodies.push_back(body);
			return celestrialBodies.size() - 1;
		}
		void Remove(CelestrialIndex index)
		{
			if (index < celestrialBodies.size())
				celestrialBodies.erase(celestrialBodies.begin() + (std::ptrdiff_t)index);
		}
		void Remove(const sRef<CelestrialType>& body)
		{
			std::erase(celestrialBodies, body);
		}
		[[nodiscard]] const sRef<CelestrialType>& Get(CelestrialIndex index) const { return celestrialBodies.at(index); }

		// First body of type DerivedCelestrialBody
		template<typename DerivedCelestrialBody>
		[[nodiscard]] sRef<DerivedCelestrialBody> GetInstance() const
		{
			for (const sRef<CelestrialType>& body : celestrialBodies)
			{
				if (sRef<DerivedCelestrialBody> derived = std::dynamic_pointer_cast<DerivedCelestrialBody>(body))
					return derived;
			}
			throw std::runtime_error("SV::CelestrialRenderer::GetInstance: no instance of the requested type");
		}

		void PopulateDistanceCache(const glm::vec3& camPos)
		{
			distanceCache.clear();
			for (size_t i = 0; i < celestrialBodies.size(); i++)
				distanceCache.insert({ glm::distance2(celestrialBodies[i]->GetTranslation(), camPos), i });
		}

		[[nodiscard]] const DistanceCache& GetDistanceCache() const { return distanceCache; }
		// Squared distance to the closest body, or infinity if there are none
		[[nodiscard]] float GetClosestDistance() const
		{
			return distanceCache.empty() ? std::numeric_limits<float>::infinity() : distanceCache.rbegin()->first;
		}

		[[nodiscard]] const std::vector<sRef<CelestrialType>>& GetCelestrialBodies() const { return celestrialBodies; }
		[[nodiscard]] size_t GetCount() const { return celestrialBodies.size(); }

	protected:
		std::vector<sRef<CelestrialType>> celestrialBodies;
		DistanceCache distanceCache;
	};
}
