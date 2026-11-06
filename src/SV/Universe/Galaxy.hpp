#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/SceneContext.hpp"

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
	// A smaller galaxy near the main one (such as M32 beside Andromeda), or a stream of stars pulled
	// out of it: a blend of three Gaussians (its halo, core and nucleus), drawn out along `stretch`
	struct GalaxyCompanion
	{
		std::string name; // Empty for no label
		glm::vec3 position{ 0.0f };
		float radius = 500.0f;        // Standard deviation of its halo (pc)
		glm::vec3 stretch{ 0.0f };    // Its halo is drawn out this far either way (one standard deviation)
		float brightness = 1.0f;      // Its halo's brightness at its centre, like the glow's
		float temperature = 4500.0f;  // The colour of its stars (K)
		float concentration = 1.0f;   // How bright its core and nucleus are (0 for a diffuse galaxy)
	};

	// The shape and look of a spiral galaxy (by default the Milky Way). Lengths are in parsecs. The
	// galactic centre is at the origin and the disc lies in the XZ plane. Seen from +y (for the
	// Milky Way, the north galactic pole), the galaxy turns clockwise, towards +z at +x (the Sun).
	struct GalaxyParameters
	{
		// The bar: a triaxial ellipsoid of old stars around the centre, plus a rounder central bulge.
		// The bar's angle is measured from +x (in the Milky Way, the Sun-centre line), with its near
		// end towards +z (44 degrees, as measured for the long bar).
		float barAngle = 44.0f; // Degrees
		float barHalfLength = 5000.0f;
		float barWidthRatio = 0.3f;
		float barHeightRatio = 0.3f;
		float bulgeRadius = 400.0f;
		// The bar's share of the central light and stars (the rest are in the round bulge)
		float barShare = 0.8f;

		// The spiral arms: trailing logarithmic spirals. The major arms (Scutum-Centaurus and
		// Perseus) leave the ends of the bar at armRadius, and the minor arms (Norma-Outer and
		// Sagittarius-Carina) lie between them. With the bar's angle, armRadius sets where the arms
		// are: as here, the Sun lies 0.8 kpc outside Sagittarius-Carina and 2.3 kpc inside Perseus.
		int armCount = 4;
		float armPitch = 14.0f;    // Degrees
		float armRadius = 4150.0f; // Where the major arms leave the bar
		float armWidth = 400.0f;   // Standard deviation across an arm
		// The minor arms' share of the young stars (and of the old disc's crowding into the arms),
		// relative to the major arms'
		float minorArmStrength = 0.2f;

		// A ring of young stars, nebulae and dust (as in Andromeda; a strength of 0 for none)
		float ringRadius = 10000.0f;
		float ringWidth = 1000.0f; // Standard deviation
		float ringStrength = 0.0f; // Relative to the arms

		// The disc: exponential in radius and height, cut off at discRadius
		float discScaleLength = 2600.0f;
		float discScaleHeight = 300.0f;
		float discRadius = 15000.0f;
		float youngScaleHeight = 80.0f; // Young stars, nebulae and their clusters
		float dustScaleHeight = 100.0f;
		// The dust lanes lie along the inner (concave) edges of the arms
		float dustLaneOffset = 200.0f;

		// The glow of the unresolved stars and gas: the brightness of each component seen face-on
		// at its brightest (the centre, or the middle of an arm)
		float bulgeGlow = 5.0f;
		float discGlow = 1.25f;
		float armGlow = 1.0f;
		float nebulaGlow = 0.8f; // Also the brightness of the H II regions drawn with the stars
		float companionGlow = 1.0f;
		// The colours of the bulge, the old disc and the young stars, as blackbodies (K)
		float bulgeTemperature = 4600.0f;
		float discTemperature = 4900.0f;
		float youngTemperature = 20000.0f;
		// Optical depth of the dust seen face-on through the middle of an arm (blue light is
		// absorbed more, so the dust reddens as well as dims)
		float dustDepth = 5.0f;

		// Smaller galaxies nearby (up to MAX_COMPANIONS)
		std::vector<GalaxyCompanion> companions;

		// Individual stars, as points (with H II regions, as small nebulae, numbering 2% of them)
		int starCount = 250000;
		float starBrightness = 0.05f;

		// Exposure of the final image
		float brightness = 0.9f;
	};

	constexpr size_t MAX_COMPANIONS = 3;

	enum class GalaxyType
	{
		MilkyWay,
		Whirlpool,
		Andromeda,
		RoseGalaxy,
		Pinwheel,
		Ngc1300,
		Triangulum,
		Sombrero,
		Count
	};

	// A labelled point in a galaxy (in galactic coordinates)
	struct GalaxyMarker
	{
		std::string name;
		glm::vec3 position{ 0.0f };
	};

	// A galaxy to show: its parameters, the points labelled in it (as well as its companions) and
	// the camera's first view of it (as it is usually seen)
	struct GalaxyPreset
	{
		std::string name;
		GalaxyParameters parameters;
		std::vector<GalaxyMarker> markers;
		float overviewPitch = 90.0f; // Degrees
		float overviewYaw = 0.0f;    // Degrees
		float overviewZoom = 40000.0f;
		// The camera's field of view (degrees): narrow for galaxies seen from afar, which show
		// little perspective
		float fieldOfView = 45.0f;
	};

	[[nodiscard]] const GalaxyPreset& GetGalaxyPreset(GalaxyType type);

	// How the galaxy is viewed this frame
	struct GalaxyView
	{
		// Where the galactic centre is in render coordinates (the camera works near the origin)
		glm::vec3 centre{ 0.0f };
		// The angle one pixel subtends (radians)
		float pixelAngle = 1e-3f;
		// The stars within holeRadius of holeCentre (galactic coordinates) fade out, where real stars
		// are drawn instead (0 for no hole)
		glm::vec3 holeCentre{ 0.0f };
		float holeRadius = 0.0f;
	};

	// Draws the galaxy into a viewport framebuffer in three passes:
	//   1. Glow: a full screen raymarch through analytic density fields (bar, bulge, disc, arms,
	//      ring, nebulae) with dust absorbing the light along the way, and the companions' light
	//      (analytic, dimmed by the disc's dust if they are behind it), at half resolution
	//   2. Stars: instanced camera facing points, each dimmed and reddened by the dust between it
	//      and the camera
	//   3. Composite: adds the two (HDR, RGBA16F) and tone maps them into the target
	class GalaxyRenderer
	{
	public:
		GalaxyRenderer(const SceneContext& context, const GalaxyParameters& parameters);
		~GalaxyRenderer();

		// Rebuilds the stars from the parameters. The glow follows the parameters each frame, but
		// stars are only placed when this is called.
		void Regenerate();

		// Outside a render pass; ends with `target` rendered (cleared first). Uses the frame
		// uniforms (set 0) for the camera.
		void Render(DH::CommandList& cmd, DH::Framebuffer& target, const GalaxyView& view);

		// Or in two steps, to add the galaxy to another scene's HDR target (a BloomCompositor's):
		// renders the glow and stars for a target of this size (outside a render pass), then adds
		// them, times `scale`, inside the pass drawing that target
		void RenderLayers(DH::CommandList& cmd, size_t width, size_t height, const GalaxyView& view);
		void AddTo(DH::CommandList& cmd, float scale);

		[[nodiscard]] size_t GetStarCount() const { return starCount; }

		GalaxyParameters parameters;

	private:
		// (Re)creates the HDR targets and the composite bind group to match the target's size
		void EnsureTargets(size_t width, size_t height);

		SceneContext context;

		sRef<DH::GPUMesh> quadMesh;
		sRef<DH::Buffer> starBuffer;
		size_t starCount = 0;

		sRef<DH::Buffer> uniformBuffer;
		sRef<DH::Texture3D> noiseTexture;
		sRef<DH::Sampler> noiseSampler;
		sRef<DH::BindGroupLayout> galaxyLayout;
		sRef<DH::BindGroup> galaxyGroup;

		sRef<DH::Framebuffer> glowTarget;
		sRef<DH::Framebuffer> starTarget;
		sRef<DH::BindGroupLayout> compositeLayout;
		sRef<DH::BindGroup> compositeGroup;

		sRef<DH::GraphicsPipeline> glowPipeline;
		sRef<DH::GraphicsPipeline> starPipeline;
		sRef<DH::GraphicsPipeline> compositePipeline;
		sRef<DH::GraphicsPipeline> addPipeline; // Created by the first AddTo
	};
}
