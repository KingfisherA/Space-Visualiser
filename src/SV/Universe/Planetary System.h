#pragma once
#include"SV-Engine/Utilities/Array.hpp"
#include"CelestrialBody.hpp"
#include"Star.hpp"
#include"PlanetNew.hpp"

namespace SV::Universe
{
	using PlanetarySystemI = size_t;
	class PlanetarySystem
	{
	public:
		PlanetarySystem(const Ref<SV::StarRenderer>& sRIn, Ref<SV::PlanetNewRenderer>& pRIn);
		//
		const PlanetarySystemI Add(Ref<SV::CelestrialBody> body);
		const void Remove(const PlanetarySystemI index);
		const void Remove(const Ref<SV::CelestrialBody>& body);
		//
		template<class A = SV::CelestrialBody>
		const Ref<A> Get(const PlanetarySystemI index) const
		{
			static_assert(std::is_base_of_v<SV::CelestrialBody, A>, "Cannot use function on any Non-Celestrial Body Types");
			return celestrialBodies[index];
		}
		template<>
		const Ref<SV::Star> Get<SV::Star>(const PlanetarySystemI index) const
		{
			Ref<SV::CelestrialBody> body = celestrialBodies[index];
			if (body->GetType() != SV::CelestrialBodyType::Star)
			{
				SV_WARN_T(SV::Universe::PlanetarySystem::Get<SV::Star>, "Object at index is not a star");
				return nullptr;
			}

			return std::static_pointer_cast<SV::Star>(body);
		}
		template<>
		const Ref<SV::PlanetNew> Get<SV::PlanetNew>(const PlanetarySystemI index) const
		{
			Ref<SV::CelestrialBody> body = celestrialBodies[index];
			if (body->GetType() != SV::CelestrialBodyType::Planet)
			{
				SV_WARN_T(SV::Universe::PlanetarySystem::Get<SV::PlanetNew>, "Object at index is not a planet");
				return nullptr;
			}

			return std::static_pointer_cast<SV::PlanetNew>(body);
		}
		//
		PlanetarySystemI Find(const Ref<SV::CelestrialBody>& body) const;
		//
		void Update(double deltaTime);
		//
	private:
		SV::Array<Ref<SV::CelestrialBody>> celestrialBodies;
		Ref<SV::StarRenderer> sRenderer;
		Ref<SV::PlanetNewRenderer> pRenderer;
	};
};