#pragma once
#include <vector>

#include "CelestrialBody.hpp"
#include "PlanetNew.hpp"
#include "Star.hpp"

namespace SV::Universe
{
	using PlanetarySystemI = size_t;

	// A set of stars and planets under mutual gravitation. Bodies added here are also added to the
	// renderer for their type.
	class PlanetarySystem
	{
	public:
		PlanetarySystem(const sRef<SV::StarRenderer>& sRIn, const sRef<SV::PlanetNewRenderer>& pRIn);

		PlanetarySystemI Add(const sRef<SV::CelestrialBody>& body);
		void Remove(PlanetarySystemI index);
		void Remove(const sRef<SV::CelestrialBody>& body);

		[[nodiscard]] const sRef<SV::CelestrialBody>& Get(PlanetarySystemI index) const { return celestrialBodies.at(index); }
		[[nodiscard]] const std::vector<sRef<SV::CelestrialBody>>& GetBodies() const { return celestrialBodies; }

		void Update(double deltaTime);

	private:
		std::vector<sRef<SV::CelestrialBody>> celestrialBodies;
		sRef<SV::StarRenderer> sRenderer;
		sRef<SV::PlanetNewRenderer> pRenderer;
	};
}
