#pragma once
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
}

namespace SV
{
	enum class CompactObjectType
	{
		BlackHole,   // Non-rotating (Schwarzschild)
		NeutronStar, // A pulsar
	};

	// The look of a black hole or a neutron star, its accretion disc and its jets or beams. Lengths are
	// in Schwarzschild radii (a black hole's horizon has a radius of 1) and times in the time light
	// takes to cross one. The object is at the origin, with the disc in the XZ plane, turning
	// anticlockwise seen from +y, and the spin axis (and jets) along the y axis.
	struct CompactObjectParameters
	{
		CompactObjectType type = CompactObjectType::BlackHole;

		// The neutron star: its radius (12 km, for 1.4 solar masses) and surface (a million kelvin,
		// which looks the same blue-white as the hottest colour in the blackbody table), and the
		// hot spots at its magnetic poles, where gas falling along the field lines lands
		float starRadius = 2.9f;
		float starTemperature = 40000.0f; // As emitted (K)
		float starBrightness = 2.0f;
		float hotSpotSize = 14.0f; // Angular radius (degrees)
		float hotSpotBrightness = 6.0f;
		// Its spin (real seconds per turn: real pulsars turn in milliseconds to seconds) and its
		// magnetic axis, tilted from the spin axis. Beams of radiation from the magnetic poles sweep
		// round with it like a lighthouse's, and it flashes when one sweeps past the camera.
		float spinPeriod = 2.0f;
		float magneticTilt = 40.0f; // Degrees
		float beamBrightness = 1.0f; // 0 for none
		float beamWidth = 7.0f;      // Half angle (degrees)
		float beamLength = 40.0f;    // The beams fade over this length
		// Glowing plasma along the closed magnetic field lines around it (0 for none)
		float magnetosphere = 0.3f;

		// The disc: from discInnerRadius (for a black hole, 3: the innermost stable circular orbit,
		// where the gas starts to fall in; for a neutron star, where its magnetic field takes over)
		// out to discOuterRadius. Its temperature follows a thin disc's, peaking just outside the
		// inner edge.
		bool showDisc = true;
		float discInnerRadius = 3.0f;
		float discOuterRadius = 14.0f;
		float discTemperature = 8500.0f; // Peak (K), as emitted
		float discBrightness = 3.0f;
		float discOpacity = 0.85f;
		// How much the gas's clumps and streaks show (0 for a smooth disc)
		float discTurbulence = 0.6f;
		// Simulated time per real second: how fast the disc turns and the jets flow
		float timeScale = 8.0f;

		// The light from the disc and star seen as it is: Doppler beamed (brighter and bluer where the
		// gas comes towards the camera) and redshifted climbing out of the gravity. Each is from 0
		// (none, as in Interstellar) to 1 (as it would be seen).
		float dopplerBeaming = 1.0f;
		float gravitationalRedshift = 1.0f;

		// Relativistic jets from the poles of the spin axis, glowing in X-rays (in false colour)
		bool showJets = true;
		float jetBrightness = 0.4f;
		float jetSpeed = 0.6f;         // Of the gas in the jets (fraction of c), for their beaming
		float jetOpeningAngle = 2.0f;  // Half angle (degrees)
		float jetLength = 30.0f;       // Each jet fades over this length

		// The sky behind: stars, and a faint glow
		float starFieldBrightness = 1.0f;
		float skyBrightness = 1.0f;

		// The glow of the brightest light (as scattered in a camera's lens), and the exposure
		float bloom = 0.5f;
		float brightness = 1.0f;

		// The fraction of the viewport's resolution the rays are traced at
		float renderScale = 1.0f;
	};

	// A black hole with a disc and jets, as in Interstellar (with its jets)
	[[nodiscard]] CompactObjectParameters GetBlackHolePreset();
	// A pulsar feeding from a disc (an X-ray pulsar), with its beams and magnetosphere
	[[nodiscard]] CompactObjectParameters GetPulsarPreset();

	// How the object is viewed this frame
	struct CompactObjectView
	{
		// Where the object is in render coordinates (the camera works near the origin)
		glm::vec3 centre{ 0.0f };
		// The angle one pixel of the viewport subtends (radians)
		float pixelAngle = 1e-3f;
	};

	// Draws a black hole or a neutron star into a viewport framebuffer. A full screen pass follows
	// each pixel's ray of light back through the curved spacetime around it (Shaders/CompactObject.frag),
	// adding the light of what it passes through (the disc, the jets or beams) and what it ends on
	// (the neutron star, or the sky), then the BloomCompositor blooms and tone maps it.
	class CompactObjectRenderer
	{
	public:
		CompactObjectRenderer(const SceneContext& context, const CompactObjectParameters& parameters);
		~CompactObjectRenderer();

		// Advances the disc's turning, the jets' flow and the star's spin by deltaTime real seconds
		void Update(double deltaTime);

		// Outside a render pass; ends with `target` rendered. Uses the frame uniforms (set 0) for
		// the camera.
		void Render(DH::CommandList& cmd, DH::Framebuffer& target, const CompactObjectView& view);

		CompactObjectParameters parameters;
		// How it is composited into the target (over the universe, as a close-up)
		CompositeMode compositeMode = CompositeMode::Replace;
		float opacity = 1.0f;

	private:
		SceneContext context;
		BloomCompositor compositor;
		double time = 0.0;
		double spinAngle = 0.0;

		sRef<DH::Buffer> uniformBuffer;
		sRef<DH::BindGroupLayout> objectLayout;
		sRef<DH::BindGroup> objectGroup;
		sRef<DH::GraphicsPipeline> tracePipeline;
	};
}
