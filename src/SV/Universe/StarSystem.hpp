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
	// How a body of the star test shines and what its surface looks like
	enum class StellarBodyKind
	{
		Star,       // Granulated (convection cells), with spots on cooler stars
		BrownDwarf, // Too small to fuse hydrogen: glowing dimly red through bands of cloud
		WhiteDwarf, // A dead star's hot, dense core, about the size of the Earth: smooth
	};

	// A star (or brown or white dwarf) of the star test. Lengths in solar radii, masses in solar
	// masses, times in days.
	struct StellarBody
	{
		std::string name;
		std::string description; // Its type, e.g. "M5.5 V red dwarf"
		StellarBodyKind kind = StellarBodyKind::Star;
		float radius = 1.0f;
		float temperature = 5772.0f; // Of its surface (K)
		float mass = 1.0f;
		float rotationPeriod = 25.0f;
		// Its convection cells (granules) to a stellar radius (fewer and larger in giants: the Sun's
		// are about 1500 km across, and a red supergiant has only a few), and how spotted it is (0 to
		// 1). Its limb darkening follows from its temperature (Shaders/Include/Photosphere.glsl).
		float granulation = 460.0f;
		float spots = 0.0f;
		// The glow around it (its chromosphere and corona, or a supergiant's dusty envelope): its
		// size (fraction of its radius) and brightness (fraction of its surface's)
		float glowSize = 0.05f;
		float glowStrength = 0.05f;
		// How often it flares (flares per minute of real time, 0 for none): red dwarfs flare often
		float flareRate = 0.0f;
		// Tints its colour (brown dwarfs look magenta: sodium and potassium absorb their yellow and green)
		glm::vec3 tint{ 1.0f };
	};

	// A node of a star system's hierarchy: a body, or a pair of nodes orbiting their common centre of
	// mass (a binary, or a binary and a third star orbiting it, and so on)
	struct StarSystemNode
	{
		std::string name;
		int body = -1; // The body at this node (or -1 for a pair)
		int first = -1; // The pair's nodes
		int second = -1;
		// The pair's orbit: their separation's semi-major axis (solar radii), eccentricity, period
		// (days), its plane's tilt from the XZ plane (degrees) and where they start along it (radians)
		float separation = 0.0f;
		float eccentricity = 0.0f;
		float period = 1.0f;
		float inclination = 0.0f;
		float phase = 0.0f;
	};

	enum class StarSystemType
	{
		RedDwarf,
		Sun,
		RedGiant,
		RedSupergiant,
		BlueGiant,
		BrownDwarf,
		WhiteDwarf,
		AlphaCentauri,
		Spica,
		Sirius,
		Algol,
		Count
	};

	// A star system to show, and the camera's first view of it
	struct StarSystemPreset
	{
		std::string name;
		std::vector<StellarBody> bodies;
		std::vector<StarSystemNode> nodes;
		int root = 0;  // The node of the whole system
		int focus = 0; // The node the camera first follows
		float overviewPitch = 15.0f; // Degrees
		float overviewZoom = 4.0f;   // Solar radii
		float brightness = 1.0f;     // The exposure
		float daysPerSecond = 1.0f;
	};

	[[nodiscard]] const StarSystemPreset& GetStarSystemPreset(StarSystemType type);

	// Up to this many bodies are drawn (Shaders/StarSystem.frag's MAX_BODIES)
	constexpr size_t MAX_STELLAR_BODIES = 4;

	// How the star system is viewed this frame
	struct StarSystemView
	{
		// Render coordinates are positions in solar radii relative to this (the camera works near
		// the origin)
		glm::dvec3 origin{ 0.0 };
		// The angle one pixel of the viewport subtends (radians)
		float pixelAngle = 1e-3f;
	};

	// Draws a star system into a viewport framebuffer. A full screen pass traces each pixel's ray to
	// the nearest body and shades its surface (limb darkening, granulation, spots and faculae, cloud
	// bands, flares, gravity darkening, and the heat of its companions' light on it). Bodies smaller than a pixel are drawn as points of the
	// same total light, with glows around them, over the sky's stars; then the BloomCompositor blooms
	// and tone maps it. The bodies follow Kepler orbits.
	class StarSystemRenderer
	{
	public:
		StarSystemRenderer(const SceneContext& context, const StarSystemPreset& preset);
		~StarSystemRenderer();

		// Shows another system (from its start)
		void SetSystem(const StarSystemPreset& preset);
		[[nodiscard]] const StarSystemPreset& GetSystem() const { return system; }

		// Advances the orbits and rotation by deltaTime real seconds (at daysPerSecond)
		void Update(double deltaTime);

		// Where a node (a body, or a pair's centre of mass) is (solar radii)
		[[nodiscard]] glm::dvec3 GetNodePosition(size_t node) const;
		// The radius of the body at a node, stretched by its companion's tides (0 for a pair)
		[[nodiscard]] float GetNodeRadius(size_t node) const;
		[[nodiscard]] double GetTime() const { return days; }

		// Outside a render pass; ends with `target` rendered. Uses the frame uniforms (set 0) for
		// the camera.
		void Render(DH::CommandList& cmd, DH::Framebuffer& target, const StarSystemView& view);

		float brightness = 1.0f;
		float bloom = 0.5f;
		float daysPerSecond = 1.0f;
		float starFieldBrightness = 1.0f;
		// Scales every body's convection cells' and spots' contrast (0 for smooth discs)
		float surfaceDetail = 1.0f;
		// Raises the light of bodies too small to see as discs, so distant stars show as points (0
		// for their true share of a pixel's light)
		float pointGain = 100.0f;

	private:
		// Places the nodes under `node`, whose centre of mass is at `centre`
		void PlaceNode(int node, const glm::dvec3& centre);
		[[nodiscard]] double NodeMass(int node) const;

		SceneContext context;
		BloomCompositor compositor;
		StarSystemPreset system;
		double days = 0.0;
		double seconds = 0.0;
		std::vector<glm::dvec3> nodePositions;
		// Each body's tidal stretch: along the direction to its partner (xyz), by w
		std::vector<glm::vec4> stretches;
		// Each body's granules' age (in their lifetimes)
		std::vector<double> granuleAges;

		sRef<DH::Buffer> uniformBuffer;
		sRef<DH::Texture3D> noiseTexture;
		sRef<DH::Sampler> noiseSampler;
		sRef<DH::BindGroupLayout> systemLayout;
		sRef<DH::BindGroup> systemGroup;
		sRef<DH::GraphicsPipeline> tracePipeline;
	};
}
