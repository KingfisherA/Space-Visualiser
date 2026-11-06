#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/BloomCompositor.hpp"
#include "SV/Graphics/SceneContext.hpp"

namespace DH
{
	class Buffer;
	class CommandList;
	class Framebuffer;
	class GraphicsPipeline;
	class Sampler;
	class Texture3D;
}

namespace SV
{
	// How a nebula's gas is shaped and lit
	enum class NebulaKind
	{
		// The expanding debris of a supernova (the Crab): a cage of thin filaments glowing in
		// hydrogen, nitrogen, sulphur and oxygen, filled with the synchrotron glow of electrons
		// spun up by the pulsar at its centre
		SupernovaRemnant,
		// Gas ionised by young, hot stars (Orion): a blister on the face of the dark molecular cloud
		// they formed from, glowing in oxygen near the stars and hydrogen further out, behind veils of dust
		HIIRegion,
		// Columns of dense, dark gas and dust (the Pillars of Creation) left standing as hot stars
		// nearby blow away the thinner gas around them. Their faces towards the stars glow where
		// they are being ionised and evaporated, against the glowing gas behind them.
		Pillars,
	};

	// A pillar of dense gas and dust: a column from its base to its head, narrowing towards the head
	struct NebulaPillar
	{
		glm::vec3 base{ 0.0f }; // pc
		glm::vec3 head{ 0.0f };
		float baseRadius = 0.2f;
		float headRadius = 0.1f;
	};

	constexpr size_t MAX_NEBULA_PILLARS = 4;

	// A star in or near a nebula. Hot stars in an H II region ionise the gas around them.
	struct NebulaStar
	{
		std::string name; // Empty for no label
		glm::vec3 position{ 0.0f }; // pc
		float brightness = 1.0f;
		float temperature = 6000.0f; // K
		float ionisingRadius = 0.0f; // pc (0 for a star that doesn't ionise the gas)
		float ionisingStrength = 0.0f;
	};

	constexpr size_t MAX_NEBULA_STARS = 16;

	// The shape and look of a nebula. Lengths are in parsecs, about its centre. Seen from +z (the
	// first view), x is right and y up.
	struct NebulaParameters
	{
		NebulaKind kind = NebulaKind::SupernovaRemnant;
		// The semi-axes of the ellipsoid it fills, and its turn about z (degrees)
		glm::vec3 size{ 2.0f };
		float tilt = 0.0f;

		// The brightness of its emission lines: hydrogen (H-alpha, red, with H-beta), doubly ionised
		// oxygen ([O III], green-blue) and ionised nitrogen and sulphur ([N II], [S II], deep red),
		// and of its continuum (synchrotron, blue-white, in a supernova remnant)
		float hydrogen = 1.0f;
		float oxygen = 1.0f;
		float sulphur = 1.0f;
		float continuum = 1.0f;

		// Optical depth of the dust (per parsec at its densest)
		float dust = 1.0f;
		// The scale of the gas's structure: noise tiles per parsec (larger for finer structure)
		float detail = 0.35f;
		// A supernova remnant's filaments: how bright and how thin (a higher power is thinner)
		float filaments = 1.0f;
		float filamentSharpness = 10.0f;
		// How far the gas's structure is pushed about (parsecs)
		float turbulence = 0.5f;

		std::vector<NebulaStar> stars;

		// Pillars (up to MAX_NEBULA_PILLARS), and the direction of the stars lighting them
		std::vector<NebulaPillar> pillars;
		glm::vec3 lightDirection{ 0.0f, 1.0f, 0.0f };

		// Shows the emission lines in the Hubble palette, as in many of Hubble's images: [S II] as
		// red, hydrogen as green and [O III] as blue (in place of their own colours)
		bool hubblePalette = false;

		float starFieldBrightness = 1.0f;
		// The glow of the brightest light, and the exposure
		float bloom = 0.25f;
		float brightness = 1.0f;
		// The fraction of the viewport's resolution the gas is raymarched at
		float renderScale = 0.75f;
	};

	enum class NebulaType
	{
		Crab,
		Orion,
		PillarsOfCreation,
		Count
	};

	// A nebula to show: its parameters and the camera's first view of it (as it is usually seen)
	struct NebulaPreset
	{
		std::string name;
		NebulaParameters parameters;
		float overviewPitch = 0.0f; // Degrees
		float overviewYaw = 0.0f;   // Degrees
		float overviewZoom = 10.0f; // pc
		float fieldOfView = 45.0f;  // Degrees
	};

	[[nodiscard]] const NebulaPreset& GetNebulaPreset(NebulaType type);

	// How the nebula is viewed this frame
	struct NebulaView
	{
		// Where the nebula's centre is in render coordinates (the camera works near the origin)
		glm::vec3 centre{ 0.0f };
		// The angle one pixel of the viewport subtends (radians)
		float pixelAngle = 1e-3f;
	};

	// Draws a nebula into a viewport framebuffer. A full screen pass raymarches each pixel's ray
	// through the gas (Shaders/Nebula.frag), adding its glow and dimming it by the dust in front, and
	// adds the stars (each dimmed by the dust between it and the camera) and the sky behind. Then
	// the BloomCompositor blooms and tone maps it.
	class NebulaRenderer
	{
	public:
		NebulaRenderer(const SceneContext& context, const NebulaParameters& parameters);
		~NebulaRenderer();

		// Advances the gas's motion by deltaTime real seconds
		void Update(double deltaTime);

		// Outside a render pass; ends with `target` rendered. Uses the frame uniforms (set 0) for
		// the camera.
		void Render(DH::CommandList& cmd, DH::Framebuffer& target, const NebulaView& view);

		NebulaParameters parameters;
		// How it is composited into the target (over the universe, as a close-up)
		CompositeMode compositeMode = CompositeMode::Replace;
		float opacity = 1.0f;

	private:
		SceneContext context;
		BloomCompositor compositor;
		double time = 0.0;

		sRef<DH::Buffer> uniformBuffer;
		sRef<DH::Texture3D> noiseTexture;
		sRef<DH::Sampler> noiseSampler;
		sRef<DH::BindGroupLayout> nebulaLayout;
		sRef<DH::BindGroup> nebulaGroup;
		sRef<DH::GraphicsPipeline> marchPipeline;
	};
}
