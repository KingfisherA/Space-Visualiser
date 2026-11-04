#pragma once
#include <glm/glm.hpp>

#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/ModalPipeline.hpp"
#include "CelestrialBody.hpp"

namespace DH
{
	class CommandList;
}

namespace SV
{
	struct SceneContext;

	class Star : public CelestrialBody
	{
	public:
		Star(const double massIn, float radiusIn, float starTempIn, const glm::vec3& starColIn = glm::vec3(1.0f)) :
			CelestrialBody(CelestrialBodyType::Star, massIn),
			radius(radiusIn),
			starTemperature(starTempIn),
			starColour(starColIn)
		{
		}

		Star(const double massIn, float radiusIn, float starTempIn, const glm::vec3& starColIn, const glm::vec3& position) :
			CelestrialBody(CelestrialBodyType::Star, massIn, position),
			radius(radiusIn),
			starTemperature(starTempIn),
			starColour(starColIn)
		{
		}

		float radius = 1.0f;
		float starTemperature = 5800.0f;
		glm::vec3 starColour;
	};

	namespace PresetStars
	{
		class Sun : public Star
		{
		public:
			Sun() : Star(1.989E30, 1.0f, 5800.0f, glm::vec3(1.0f)) {}
			explicit Sun(const glm::vec3& posIn) : Star(1.989E30, 1.0f, 5800.0f, glm::vec3(1.0f), posIn) {}
		};
	}

	// Draws stars (noise textured spheres) and their coronas (additive camera facing quads)
	class StarRenderer : public CelestrialRenderer<SV::Star>
	{
	public:
		explicit StarRenderer(const SceneContext& context);

		// Must be inside the scene render pass with the frame bind group bound at set 0
		void Render(DH::CommandList& cmd, DH::PolygonMode polygonMode);

	private:
		std::vector<sRef<SV::Star>>& stars = CelestrialRenderer<SV::Star>::celestrialBodies;

		sRef<DH::GPUMesh> sphereMesh;
		sRef<DH::GPUMesh> quadMesh;

		ModalPipeline starPipeline;
		ModalPipeline coronaPipeline;
	};
}
