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
	// The colour of a stylised star at the given temperature (K): its blackbody colour relative to the
	// Sun's (so the Sun is white), softened a little, with its brightest channel 1 (mirrors
	// StylisedColour in Shaders/Include/StylisedStar.glsl)
	[[nodiscard]] glm::vec3 StylisedStarColour(float temperature);
	// The luminance of a blackbody's surface at the given temperature (K), relative to the Sun's
	// (5772 K): how bright it looks per unit area, which falls far faster than its total light as it
	// cools (most of a cool star's light is infrared)
	[[nodiscard]] float BlackbodyLuminance(float temperature);

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
		// Convection cells (granules) across a stellar radius: the Sun's are about 1500 km across
		float granulation = 460.0f;
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

	// How a star's surface is shown
	enum class StarSurfaceView
	{
		VisibleLight, // The photosphere, as through a solar filter (Shaders/Include/Photosphere.glsl)
		Ultraviolet,  // The chromosphere and corona above it, as SDO images them in extreme ultraviolet
		Stylised,     // A cinematic take on a star, coloured by its temperature, and its corona as an eclipse shows it (Shaders/Include/StylisedStar.glsl)
	};

	// Draws stars (spheres, their photospheres shaded in Star.frag, with flares) and their coronas,
	// loops, chromospheres and prominences (additive camera facing quads), and the glare of stars too
	// small on screen to show them (a lens flare)
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

		// Draws one star that isn't among this renderer's bodies (one of the catalogue's, close up):
		// its corona, then its surface over it, with `transform` (model to world, scaled to its radius)
		// in the frame bound at set 0. `brightness` fades it in (0 to 1). Must be inside a render pass
		// whose depth has been cleared.
		void RenderCloseUp(DH::CommandList& cmd, const glm::mat4& transform, float temperature, float brightness);
		// Draws the flare of one such star, as RenderFlares does the Sun's, with the frame bound at set
		// 0 (for its exposure). Its disc on screen is centred at `centre` with radius `radius`, in
		// view units (normalised device coordinates with x times `aspect`, so 1 is half the viewport's
		// height), where `pixel` is a pixel. `distanceAU` is the distance it would be at to look as
		// bright as the Sun does from there (its size follows that), and `presence` fades it in.
		void RenderFlare(DH::CommandList& cmd, const glm::vec2& centre, float radius, float aspect, float pixel, double distanceAU,
						 float temperature, float presence) const;

		// Brightness of the hydrogen (Balmer line) emission of the chromosphere and prominences
		float hydrogenEmission = 1.0f;
		// Brightness of solar flares on the disc and in the loops over the limb (0 for none)
		float solarFlares = 1.0f;
		StarSurfaceView surfaceView = StarSurfaceView::Stylised;
		// Contrast of sunspots and faculae in visible light (0 for none)
		float sunspots = 1.0f;
		// Brightness of the stars' flares (0 for none)
		float flareIntensity = 1.0f;

	private:
		// The colour a star's surface and corona are drawn in: its photosphere's, or its stylised colour
		[[nodiscard]] glm::vec3 GetSurfaceColour(float temperature) const;
		void DrawCorona(DH::CommandList& cmd, const glm::mat4& transform, const glm::vec3& colour, float brightness) const;
		void DrawSurface(DH::CommandList& cmd, const glm::mat4& transform, const glm::vec3& colour, float temperature, float granulation,
						 float brightness) const;
		// How strongly a star's flare shows (before anything in front of it dims it): fading in as its
		// disc shrinks on screen, and out as it leaves the view (in view units, as RenderFlare's)
		[[nodiscard]] float GetFlareStrength(const glm::vec2& centre, float radius, float aspect, float pixel) const;
		// The colour of a star's flare (with a luminance of 1), from its photosphere's colour
		[[nodiscard]] glm::vec3 GetFlareColour(float temperature, const glm::vec3& photosphere) const;
		// With the flare pipeline bound
		void DrawFlare(DH::CommandList& cmd, const glm::vec2& centre, float radius, float aspect, float pixel, float strength,
					   double distanceAU, const glm::vec3& colour) const;

		std::vector<sRef<SV::Star>>& stars = CelestrialRenderer<SV::Star>::celestrialBodies;

		sRef<DH::GPUMesh> sphereMesh;
		sRef<DH::GPUMesh> quadMesh;

		ModalPipeline starPipeline;
		ModalPipeline coronaPipeline;
		sRef<DH::GraphicsPipeline> flarePipeline;
	};
}
