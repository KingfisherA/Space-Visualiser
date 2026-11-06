#pragma once
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/BloomCompositor.hpp"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Universe/Catalogue.hpp"

namespace DH
{
	class Buffer;
	class CommandList;
	class Framebuffer;
	class GPUMesh;
	class GraphicsPipeline;
	class Sampler;
	class Texture3D;
}

namespace SV
{
	class CompactObjectRenderer;
	class GalaxyRenderer;
	class NebulaRenderer;

	// How the universe is viewed this frame. Positions are in parsecs from the Sun along the world's
	// axes (as the catalogue's), in doubles: the camera may be anywhere from inside the solar system
	// to hundreds of megaparsecs away.
	struct UniverseView
	{
		glm::dvec3 camera{ 0.0 };
		// The render origin (the camera's orbit centre), which the Milky Way is drawn relative to,
		// and the camera's position from it (precise however far the origin is from the Sun)
		glm::dvec3 origin{ 0.0 };
		glm::dvec3 offset{ 0.0 };
		glm::mat3 rotation{ 1.0f };  // World to view (the camera's orientation)
		float fieldOfView = 0.785f;  // Vertical (radians)
		float zoom = 1.0f;           // The camera's distance from its centre (pc)
		double time = 0.0;           // Seconds, for the pulsars
		double deltaTime = 0.0;      // Since the last frame (s), for the close-ups' motion
		// How much of the Sun to draw as a point: 0 while the solar system draws it
		float sunVisibility = 0.0f;
	};

	// Draws the catalogue's universe into a viewport framebuffer: the Milky Way (the galaxy model,
	// placed and turned to match the real one around Sagittarius A*), the other galaxies, the
	// nebulae, the stars and the neutron stars and black holes, into an HDR target that is bloomed
	// and tone mapped (BloomCompositor).
	//
	// Stars are points of their true brightness (flux preserving Gaussians about a pixel wide),
	// with a gain as the camera zooms out past the nearer stars so they still show. Galaxies and
	// nebulae are sprites of their surface brightness (galaxies' discs in 3D, turned as they are on
	// the sky), turning into points as they shrink below a few pixels. Neutron stars and black holes
	// give out next to no light that could be seen: they are drawn as markers, pulsars pulsing.
	//
	// Close up, the test scenes' renderers take over, turned to look from the Sun as their presets
	// do (as these objects are usually seen): the galaxies with presets (Andromeda, Triangulum, the
	// Whirlpool, the Pinwheel, the Sombrero, NGC 1300 and the Rose Galaxy) as the galaxy model, the
	// Crab, Orion and Eagle nebulae as the nebula model, and any black hole or neutron star the
	// camera is centred on as the black hole or pulsar, each fading in over its sprite or marker.
	class UniverseRenderer
	{
	public:
		UniverseRenderer(const SceneContext& context, const Catalogue& catalogue);
		~UniverseRenderer();

		// Outside a render pass; ends with `target` rendered (overwritten)
		void Render(DH::CommandList& cmd, DH::Framebuffer& target, const UniverseView& view);

		[[nodiscard]] size_t GetStarCount() const { return starCount; }

		// How prominent a landmark is in this view, for choosing which to label: negative if it
		// can't be seen (or its layer is hidden), otherwise higher for the brighter, larger and
		// better known. For picking (clicking on the sky), anything that can be seen at all counts.
		[[nodiscard]] float GetProminence(const CatalogueLandmark& landmark, const UniverseView& view, float pixelAngle, bool picking = false) const;

		// The closest the camera may zoom on a centre (pc): outside the horizon of a black hole or
		// the surface of a neutron star there (0 if there is none)
		[[nodiscard]] double GetMinZoom(const glm::dvec3& centre) const;
		// The zoom (pc) to see a landmark at: close up for black holes and neutron stars (0 for the
		// default)
		[[nodiscard]] double GetLandmarkZoom(const CatalogueLandmark& landmark) const;

		// The exposure of the whole image, and the brightness of each layer relative to it
		float exposure = 12.0f;
		float bloom = 0.6f;
		float starBrightness = 1.0f;
		// Raises the light of stars as the camera zooms out past them (0 for their true brightness)
		bool zoomGain = true;
		float milkyWayBrightness = 1.0f;
		float galaxyBrightness = 1.0f;
		float nebulaBrightness = 1.0f;
		float markerBrightness = 1.0f;
		bool showStars = true;
		bool showMilkyWay = true;
		bool showGalaxies = true;
		bool showNebulae = true;
		bool showNeutronStars = true;
		bool showBlackHoles = true;
		bool showCloseUps = true;

	private:
		// A camera of its own for a renderer drawing in its own axes and units
		struct LocalFrame
		{
			sRef<DH::Buffer> buffer;
			sRef<DH::BindGroup> group;
		};
		// An object with a test scene's preset, and the turn from its model's axes to the world's
		struct Feature
		{
			glm::dvec3 position{ 0.0 };
			float radius = 0.0f; // pc
			int preset = 0;
			glm::dmat3 modelToWorld{ 1.0 };
			// Nebulae: the region whose other sprites the close-up hides (pc, across the line of
			// sight from the Sun and along it)
			float hideRadius = 0.0f;
			double hideDepth = 0.0;
		};
		// A black hole or neutron star, for close-ups
		struct CompactPoint
		{
			glm::dvec3 position{ 0.0 };
			double schwarzschildRadius = 0.0; // pc
			bool blackHole = false;
		};

		// What is shown close up this frame, and how far each has faded in (0 to 1)
		struct CloseUps
		{
			const Feature* galaxy = nullptr;
			float galaxyPresence = 0.0f;
			const Feature* nebula = nullptr;
			float nebulaPresence = 0.0f;
			const CompactPoint* compact = nullptr;
			float compactPresence = 0.0f;
		};

		// The universe's HDR scene (with the galaxy close up), outside a render pass
		void RenderScene(DH::CommandList& cmd, DH::Framebuffer& scene, const UniverseView& view, float pixelAngle, float aspect, const CloseUps& closeUps);
		void RenderMilkyWay(DH::CommandList& cmd, const DH::Framebuffer& scene, const UniverseView& view, float pixelAngle);
		[[nodiscard]] LocalFrame CreateLocalFrame() const;
		// Sets a frame's camera for a model centred at `centre` (pc), turned by modelToWorld and in
		// units of `unit` pc, where `extent` (in its units) takes in all of it
		void UpdateLocalFrame(const LocalFrame& frame, const UniverseView& view, const glm::dmat3& modelToWorld, const glm::dvec3& centre,
							  double unit, float extent, float aspect) const;
		// Whether a position is within the region a nebula close-up hides the other sprites of
		[[nodiscard]] static bool IsInNebulaRegion(const Feature& nebula, const glm::dvec3& position);
		// The black hole or neutron star the camera is centred on, if any
		[[nodiscard]] const CompactPoint* FindCompactAt(const glm::dvec3& centre) const;
		// The galaxy or nebula with a preset that is largest on screen, and its size (pixels)
		[[nodiscard]] static const Feature* FindFeature(const std::vector<Feature>& features, const UniverseView& view, float pixelAngle,
														float& radiusPixels);

		SceneContext context;
		BloomCompositor compositor;

		sRef<DH::GPUMesh> quadMesh;
		sRef<DH::Buffer> pointBuffer;  // Stars, then compact sources, then the Sun
		size_t starCount = 0;
		size_t compactCount = 0;
		sRef<DH::Buffer> galaxyBuffer;
		size_t galaxyCount = 0;
		sRef<DH::Buffer> nebulaBuffer;
		size_t nebulaCount = 0;

		sRef<DH::Buffer> uniformBuffer;
		sRef<DH::Texture3D> noiseTexture;
		sRef<DH::Sampler> noiseSampler;
		sRef<DH::BindGroupLayout> universeLayout;
		sRef<DH::BindGroup> universeGroup;
		sRef<DH::GraphicsPipeline> pointPipeline;
		sRef<DH::GraphicsPipeline> spritePipeline;

		// The Milky Way, with its own frame uniforms (its camera, in its own axes and parsecs)
		uRef<GalaxyRenderer> milkyWay;
		sRef<DH::Buffer> galaxyFrameBuffer;
		sRef<DH::BindGroup> galaxyFrameGroup;
		glm::dmat3 galaxyToWorld{ 1.0 };

		// Close-ups: the objects that have them, and the renderer of each kind shown (created as
		// the camera nears one)
		std::vector<Feature> galaxyFeatures;
		std::vector<Feature> nebulaFeatures;
		std::vector<CompactPoint> compactPoints;
		uRef<GalaxyRenderer> featuredGalaxy;
		int featuredGalaxyPreset = -1;
		LocalFrame featuredGalaxyFrame;
		uRef<NebulaRenderer> featuredNebula;
		int featuredNebulaPreset = -1;
		LocalFrame featuredNebulaFrame;
		// The nebula shown close up in the last frame, and how far it had faded in (for its labels)
		const Feature* hiddenNebula = nullptr;
		float hiddenNebulaPresence = 0.0f;
		uRef<CompactObjectRenderer> compactCloseUp;
		bool compactCloseUpIsBlackHole = false;
		LocalFrame compactFrame;
	};
}
