#pragma once
#include <array>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
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

	// Colour of a blackbody at the given temperature (K): linear sRGB with a luminance of 1
	glm::vec3 BlackbodyColour(float temperature);

	// BlackbodyColour at this many temperatures, spaced evenly in log temperature between the two
	// (K), for shaders to look up (mirrors BLACKBODY_* in Shaders/Include/Procedural.glsl)
	constexpr size_t BLACKBODY_TABLE_SIZE = 32;
	constexpr float BLACKBODY_TABLE_MIN = 1000.0f;
	constexpr float BLACKBODY_TABLE_MAX = 40000.0f;
	[[nodiscard]] std::array<glm::vec4, BLACKBODY_TABLE_SIZE> BlackbodyTable();

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

		// The photosphere's colour: a blackbody at the star's temperature, tinted by starColour
		[[nodiscard]] glm::vec3 GetPhotosphereColour() const { return BlackbodyColour(starTemperature) * starColour; }

		// The star is drawn with the radius of its scale
		float radius = 1.0f;
		float starTemperature = 5772.0f;
		glm::vec3 starColour;
	};

	namespace PresetStars
	{
		class Sun : public Star
		{
		public:
			Sun() : Star(1.98847E30, 1.0f, 5772.0f, glm::vec3(1.0f)) {}
		};
	}

	// How the scene is viewed, for drawing flares
	struct FlareView
	{
		glm::mat4 viewProjection;
		glm::vec3 cameraPosition;
		float projectionScale; // The projection's [1][1]: 1 / tan(half the vertical field of view)
		glm::vec2 viewportSize; // Pixels
	};

	// Draws stars (noise textured spheres, with flares) and their coronas, loops, chromospheres and
	// prominences (additive camera facing quads), and the glare of stars too small on screen to show them (a lens flare)
	class StarRenderer : public CelestrialRenderer<SV::Star>
	{
	public:
		explicit StarRenderer(const SceneContext& context);

		// Must be inside the scene render pass with the frame bind group bound at set 0
		void Render(DH::CommandList& cmd, DH::PolygonMode polygonMode);

		// Draws the flares over everything else, so call it last in the scene pass. Each fades in
		// as its star shrinks on screen, and is dimmed by the part of the star's disc that
		// `occluders` (the scene's other bodies) hide.
		void RenderFlares(DH::CommandList& cmd, const FlareView& view, const std::vector<sRef<CelestrialBody>>& occluders);

		// Brightness of the hydrogen (Balmer line) emission of the chromosphere and prominences
		float hydrogenEmission = 1.0f;
		// Brightness of solar flares on the disc and in the loops over the limb (0 for none)
		float solarFlares = 1.0f;
		// Brightness of the flare (0 for none)
		float flareIntensity = 1.0f;

	private:
		std::vector<sRef<SV::Star>>& stars = CelestrialRenderer<SV::Star>::celestrialBodies;

		sRef<DH::GPUMesh> sphereMesh;
		sRef<DH::GPUMesh> quadMesh;

		ModalPipeline starPipeline;
		ModalPipeline coronaPipeline;
		sRef<DH::GraphicsPipeline> flarePipeline;
	};
}
