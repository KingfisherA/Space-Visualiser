#pragma once
#include"SV/OpenGL/OpenGL Context.hpp"
#include"SV/Scene/Transformation.hpp"
#include"SV/Utilities/SVMemory.hpp"
#include"SV/Utilities/Errors.hpp"
#include"SV/Utilities/Array.hpp"
#include"SV/Scene/Camera.hpp"

#include<type_traits>
#include<map>

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

	class CelestrialBody abstract : public Transformation
	{
	public:
		CelestrialBody(const CelestrialBodyType typeIn, const double massIn) : Transformation(), mytype(typeIn), mass(massIn) {}
		CelestrialBody(const CelestrialBodyType typeIn, const double massIn, const glm::vec3& posIn) : Transformation(posIn), mytype(typeIn), mass(massIn) {}
		CelestrialBody(const CelestrialBody& cb) : Transformation((const Transformation&)cb), mytype(cb.mytype), mass(cb.mass) {}

		const CelestrialBodyType GetType() const noexcept { return mytype; }
		const double GetMass() const noexcept { return mass; }
		const glm::dvec3& GetVelocity() const noexcept { return velocity; }

		void SetVelocity(const glm::dvec3& vel) { velocity = vel; }

	protected:
		friend class SV::Universe::PlanetarySystem;
		using Transformation::translation;

		glm::dvec3 velocity = { 0.0, 0.0, 0.0 };
		double mass;
		CelestrialBodyType mytype;
	};

	using CelestrialIndex = size_t;

	template<class CelestrialType>
	class CelestrialRenderer abstract
	{
	public:
		using DistanceCache = std::map<float, size_t, std::greater<float>>;
		static_assert(std::is_base_of_v<CelestrialBody, CelestrialType>, "CelestrialType must be a derived class of CelestrialBody");
	protected:
		SV::Array<Ref<CelestrialType>> celestrialBodies;
		DistanceCache distanceCache;
	public:

		CelestrialRenderer() : celestrialBodies(), distanceCache()
		{}
		//
		CelestrialIndex Add(Ref<CelestrialType> body)
		{
			return celestrialBodies.Add(body);
		}
		void Remove(CelestrialIndex index)
		{
			if ((size_t)index + 1 < celestrialBodies.GetCount()) celestrialBodies.Remove(index);
		}
		void Remove(const Ref<CelestrialType>& body)
		{
			celestrialBodies.Remove(body);
		}
		const Ref<CelestrialType> operator[](CelestrialIndex index) const { return celestrialBodies[index]; }
		const Ref<CelestrialType>& Get(CelestrialIndex index) const { return celestrialBodies[index]; }
		template<typename DerivedCelestrialBody, bool useCache = true>
		const unsigned int GetInstanceIndex()
		{
			return celestrialBodies.GetInstanceIndex<DerivedCelestrialBody, useCache>();
		}
		template<typename DerivedCelestrialBody, bool useCache = true>
		const Ref<DerivedCelestrialBody> GetInstance()
		{
			unsigned int index = celestrialBodies.GetInstanceIndex<DerivedCelestrialBody, useCache>();
			SV_ASSERT_T(index == IndexUndefined, SV::CelestrialRenderer<T>::GetInstance<DerivedCelestrialBody>, "Cannot find instance of template type.");
			return std::static_pointer_cast<DerivedCelestrialBody>(celestrialBodies[index]);
		}
		//
		void PopulateDistanceCache(const glm::vec3& camPos)
		{
			distanceCache.clear();

			for (size_t i = 0; i < celestrialBodies.GetCount(); i++)
			{
				distanceCache.insert({
					glm::distance2(celestrialBodies[i]->GetTranslation(), camPos),
					i });
			}
		}
		//
		const DistanceCache& GetDistanceCache() { return distanceCache; }
		const float GetClosestDistance() const
		{
			SV_ASSERT_T(distanceCache.size() == 0, SV::CelestrialRenderer<T>::GetClosestDistance, "Distance Cache is empty. Populate it before querying data.");
			return (*distanceCache.rbegin()).first;
		}
		const Ref<CelestrialType> GetClosestObject() const
		{
			SV_ASSERT_T(distanceCache.size() == 0, SV::CelestrialRenderer<T>::GetClosestObject, "Distance Cache is empty. Populate it before querying data.");
			return celestrialBodies[(*distanceCache.rbegin()).second];
		}
		SV::Array<Ref<CelestrialType>>& GetCelestrialBodies() { return celestrialBodies; }
		const SV::Array<Ref<CelestrialType>>& GetCelestrialBodies() const { return celestrialBodies; }
		//
		const unsigned int GetCount() const { return celestrialBodies.GetCount(); }
		//
	};

};