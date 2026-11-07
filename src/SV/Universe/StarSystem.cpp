#include "StarSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "SV/Graphics/NoiseVolume.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "Star.hpp"

namespace SV
{
	namespace
	{
		constexpr size_t MAX_BODIES = MAX_STELLAR_BODIES;

		// Mirrors StarSystemUniforms in Shaders/StarSystem.frag (std140)
		struct StarSystemUniforms
		{
			glm::vec4 info; // x = body count, y = pixel angle, z = star field brightness, w = surface detail
			std::array<glm::vec4, MAX_BODIES> position; // xyz = centre (render coordinates), w = radius
			std::array<glm::vec4, MAX_BODIES> surface;  // x = temperature, y = kind, z = granulation, w = spots
			std::array<glm::vec4, MAX_BODIES> stretch;  // xyz = the tides' axis, w = stretch along it
			std::array<glm::vec4, MAX_BODIES> look;     // x = granules' age (lifetimes), y = glow size, z = glow strength, w = rotation (radians)
			std::array<glm::vec4, MAX_BODIES> tint;     // rgb = tint, w = surface luminance (relative to the Sun's)
			std::array<glm::vec4, MAX_BODIES> flare;    // xyz = where (in the body's turning frame), w = brightness
			glm::vec4 display;                          // x = point star gain
			std::array<glm::vec4, BLACKBODY_TABLE_SIZE> blackbody;
		};

		constexpr double TAU = 2.0 * std::numbers::pi;
		// Tides stretch a star along the line to its partner by about this times the partner's mass
		// over its own times (its radius over their separation) cubed, up to MAX_STRETCH
		constexpr float TIDAL_STRETCH = 1.5f;
		constexpr float MAX_STRETCH = 1.35f;
		// A flare: how long it brightens and fades (real seconds), and how bright it is
		constexpr double FLARE_RISE = 0.3;
		constexpr double FLARE_FADE = 1.5;
		constexpr float FLARE_BRIGHTNESS = 3.0f;
		// Granules last about ten minutes on the Sun, and longer in proportion to their size on other
		// stars (years on a red supergiant), but evolve no faster than this many lifetimes per real
		// second however fast time passes
		constexpr double SOLAR_GRANULE_LIFETIME = 10.0 / (24.0 * 60.0); // Days
		constexpr double SOLAR_GRANULES = 460.0;                       // To a radius
		constexpr double MAX_GRANULE_RATE = 0.3;
		// The solar radius and the astronomical unit, in kilometres
		constexpr double SOLAR_RADIUS_KM = 695700.0;
		constexpr double AU_KM = 149597870.7;
		constexpr float AU = (float)(AU_KM / SOLAR_RADIUS_KM);

		uint32_t Mix(uint32_t h)
		{
			h ^= h >> 16;
			h *= 0x7feb352du;
			h ^= h >> 15;
			h *= 0x846ca68bu;
			h ^= h >> 16;
			return h;
		}

		float Random(const uint32_t seed)
		{
			return (float)(Mix(seed) & 0xffffffu) / (float)0xffffff;
		}

		// A single body, at the root
		StarSystemPreset Single(const std::string& name, const StellarBody& body, const float zoom, const float brightness, const float daysPerSecond)
		{
			StarSystemPreset preset;
			preset.name = name;
			preset.bodies = { body };
			preset.nodes = { { body.name, 0 } };
			preset.overviewZoom = zoom;
			preset.brightness = brightness;
			preset.daysPerSecond = daysPerSecond;
			return preset;
		}

		std::array<StarSystemPreset, (size_t)StarSystemType::Count> CreatePresets()
		{
			std::array<StarSystemPreset, (size_t)StarSystemType::Count> presets;

			// The nearest star to the Sun: a small, cool, spotted flare star
			StellarBody proxima{ "Proxima Centauri", "M5.5 V red dwarf", StellarBodyKind::Star, 0.154f, 3042.0f, 0.122f, 83.0f, 690.0f, 0.35f };
			proxima.flareRate = 6.0f;
			presets[(size_t)StarSystemType::RedDwarf] = Single("Red dwarf (Proxima Centauri)", proxima, 0.6f, 17.0f, 2.0f);

			const StellarBody sun{ "Sun", "G2 V main sequence star", StellarBodyKind::Star, 1.0f, 5772.0f, 1.0f, 25.4f, 460.0f, 0.1f };
			presets[(size_t)StarSystemType::Sun] = Single("Main sequence star (the Sun)", sun, 4.0f, 0.39f, 1.0f);

			StellarBody aldebaran{ "Aldebaran", "K5 III red giant", StellarBodyKind::Star, 44.0f, 3900.0f, 1.16f, 520.0f, 18.0f, 0.0f };
			aldebaran.glowSize = 0.1f;
			presets[(size_t)StarSystemType::RedGiant] = Single("Red giant (Aldebaran)", aldebaran, 170.0f, 3.0f, 20.0f);

			// A few huge convection cells, and a dusty envelope
			StellarBody betelgeuse{ "Betelgeuse", "M1-2 Ia red supergiant", StellarBodyKind::Star, 760.0f, 3600.0f, 17.0f, 13000.0f, 1.8f, 0.0f };
			betelgeuse.glowSize = 0.6f;
			betelgeuse.glowStrength = 0.08f;
			presets[(size_t)StarSystemType::RedSupergiant] = Single("Red supergiant (Betelgeuse)", betelgeuse, 3000.0f, 5.0f, 500.0f);

			const StellarBody rigel{ "Rigel", "B8 Ia blue supergiant", StellarBodyKind::Star, 79.0f, 12100.0f, 21.0f, 160.0f, 25.0f, 0.0f };
			presets[(size_t)StarSystemType::BlueGiant] = Single("Blue giant (Rigel)", rigel, 300.0f, 0.037f, 5.0f);

			// About Jupiter's size, turning in 5.3 hours
			StellarBody luhman{ "Luhman 16 B", "T0.5 brown dwarf", StellarBodyKind::BrownDwarf, 0.099f, 1210.0f, 0.03f, 0.22f, 0.0f, 0.0f };
			luhman.glowStrength = 0.0f;
			luhman.tint = { 1.0f, 0.4f, 0.85f };
			presets[(size_t)StarSystemType::BrownDwarf] = Single("Brown dwarf (Luhman 16 B)", luhman, 0.4f, 2.0e6f, 0.05f);

			// About the Earth's size, with the Sun's mass
			StellarBody siriusB{ "Sirius B", "DA2 white dwarf", StellarBodyKind::WhiteDwarf, 0.0084f, 25000.0f, 1.02f, 1.0f, 0.0f, 0.0f };
			siriusB.glowStrength = 0.02f;
			presets[(size_t)StarSystemType::WhiteDwarf] = Single("White dwarf (Sirius B)", siriusB, 0.035f, 0.0092f, 0.2f);

			// Two Sun-like stars 23 AU apart on an eccentric 80 year orbit
			{
				StarSystemPreset& preset = presets[(size_t)StarSystemType::AlphaCentauri];
				preset.name = "Binary (Alpha Centauri A and B)";
				preset.bodies = {
					{ "Alpha Centauri A", "G2 V main sequence star", StellarBodyKind::Star, 1.22f, 5790.0f, 1.08f, 22.0f, 400.0f, 0.1f },
					{ "Alpha Centauri B", "K1 V main sequence star", StellarBodyKind::Star, 0.86f, 5260.0f, 0.91f, 36.0f, 530.0f, 0.2f },
				};
				preset.nodes = {
					{ "Alpha Centauri AB", -1, 1, 2, 23.4f * AU, 0.52f, 29200.0f, 0.0f, 2.0f },
					{ "Alpha Centauri A", 0 },
					{ "Alpha Centauri B", 1 },
				};
				preset.overviewPitch = 35.0f;
				preset.overviewZoom = 15000.0f;
				preset.brightness = 0.39f;
				preset.daysPerSecond = 1500.0f;
			}

			// Two blue stars 0.12 AU apart, turning in 4 days: stretched by each other's tides
			{
				StarSystemPreset& preset = presets[(size_t)StarSystemType::Spica];
				preset.name = "Close binary (Spica)";
				preset.bodies = {
					{ "Spica A", "B1 III-IV blue giant", StellarBodyKind::Star, 7.47f, 25300.0f, 11.4f, 4.0f, 20.0f, 0.0f },
					{ "Spica B", "B2 V main sequence star", StellarBodyKind::Star, 3.64f, 20900.0f, 7.2f, 4.0f, 25.0f, 0.0f },
				};
				preset.nodes = {
					{ "Spica AB", -1, 1, 2, 26.6f, 0.13f, 4.0145f, 0.0f, 0.5f },
					{ "Spica A", 0 },
					{ "Spica B", 1 },
				};
				preset.overviewPitch = 12.0f;
				preset.overviewZoom = 75.0f;
				preset.brightness = 0.01f;
				preset.daysPerSecond = 0.4f;
			}

			// The brightest star in the night sky and its white dwarf, 20 AU apart on a 50 year orbit
			{
				StarSystemPreset& preset = presets[(size_t)StarSystemType::Sirius];
				preset.name = "Binary with a white dwarf (Sirius A and B)";
				preset.bodies = {
					{ "Sirius A", "A0-1 V main sequence star", StellarBodyKind::Star, 1.711f, 9940.0f, 2.06f, 5.0f, 30.0f, 0.0f },
					siriusB,
				};
				preset.nodes = {
					{ "Sirius AB", -1, 1, 2, 19.8f * AU, 0.59f, 18300.0f, 0.0f, 1.0f },
					{ "Sirius A", 0 },
					{ "Sirius B", 1 },
				};
				preset.overviewPitch = 40.0f;
				preset.overviewZoom = 12000.0f;
				preset.brightness = 0.064f;
				preset.daysPerSecond = 1000.0f;
			}

			// An eclipsing pair (a hot blue star and a cooler subgiant, 0.06 AU apart, eclipsing each
			// other every 2.87 days), and a third star orbiting them every 680 days, at right angles
			{
				StarSystemPreset& preset = presets[(size_t)StarSystemType::Algol];
				preset.name = "Trinary (Algol)";
				preset.bodies = {
					{ "Algol A", "B8 V main sequence star", StellarBodyKind::Star, 2.73f, 12550.0f, 3.17f, 2.867f, 25.0f, 0.0f },
					{ "Algol B", "K0 IV subgiant", StellarBodyKind::Star, 3.48f, 4500.0f, 0.70f, 2.867f, 120.0f, 0.15f },
					{ "Algol C", "A7 m main sequence star", StellarBodyKind::Star, 1.73f, 7500.0f, 1.76f, 2.0f, 360.0f, 0.0f },
				};
				preset.nodes = {
					{ "Algol", -1, 1, 4, 2.69f * AU, 0.23f, 680.0f, 0.0f, 0.0f },
					{ "Algol AB", -1, 2, 3, 14.0f, 0.0f, 2.867f, 90.0f, 1.2f },
					{ "Algol A", 0 },
					{ "Algol B", 1 },
					{ "Algol C", 2 },
				};
				preset.focus = 1;
				preset.overviewPitch = 8.0f;
				preset.overviewZoom = 45.0f;
				preset.brightness = 0.061f;
				preset.daysPerSecond = 0.3f;
			}
			return presets;
		}
	}

	const StarSystemPreset& GetStarSystemPreset(const StarSystemType type)
	{
		static const std::array<StarSystemPreset, (size_t)StarSystemType::Count> presets = CreatePresets();
		return presets[std::min((size_t)type, presets.size() - 1)];
	}

	StarSystemRenderer::StarSystemRenderer(const SceneContext& contextIn, const StarSystemPreset& preset) :
		context(contextIn),
		compositor(contextIn)
	{
		const StarSystemUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(StarSystemUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);
		noiseTexture = GetNoiseVolume();
		noiseSampler = CreateNoiseSampler();

		// Set 1 of the trace: the bodies and the noise
		systemLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler3D, DH::ShaderStage::Fragment } });
		systemGroup = DH::BindGroup::Create(systemLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, noiseTexture, noiseSampler) });

		DH::GraphicsPipelineDesc traceDesc;
		traceDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle with each pixel's NDC
		traceDesc.fragmentShader = Shaders::Create("StarSystem.frag");
		traceDesc.depth = { false, false, DH::CompareOp::Always };
		traceDesc.blend = DH::BlendPreset::None;
		traceDesc.targetLayout = BloomCompositor::GetSceneLayout();
		traceDesc.bindGroupLayouts = { context.frameLayout, systemLayout };
		tracePipeline = DH::GraphicsPipeline::Create(traceDesc);

		SetSystem(preset);
	}

	StarSystemRenderer::~StarSystemRenderer() = default;

	void StarSystemRenderer::SetSystem(const StarSystemPreset& preset)
	{
		system = preset;
		brightness = preset.brightness;
		daysPerSecond = preset.daysPerSecond;
		days = 0.0;
		seconds = 0.0;
		granuleAges.assign(system.bodies.size(), 0.0);
		Update(0.0);
	}

	double StarSystemRenderer::NodeMass(const int node) const
	{
		const StarSystemNode& n = system.nodes[(size_t)node];
		if (n.body >= 0)
			return system.bodies[(size_t)n.body].mass;
		return NodeMass(n.first) + NodeMass(n.second);
	}

	void StarSystemRenderer::PlaceNode(const int node, const glm::dvec3& centre)
	{
		nodePositions[(size_t)node] = centre;
		const StarSystemNode& n = system.nodes[(size_t)node];
		if (n.body >= 0)
			return;

		// Kepler: the mean anomaly, then the eccentric anomaly by Newton's method
		const double e = n.eccentricity;
		const double meanAnomaly = std::fmod(TAU * days / std::max((double)n.period, 1e-6) + n.phase, TAU);
		double E = e < 0.8 ? meanAnomaly : std::numbers::pi;
		for (int i = 0; i < 8; i++)
			E -= (E - e * std::sin(E) - meanAnomaly) / (1.0 - e * std::cos(E));
		// The second's position from the first, in the orbit's plane, then tilted about x
		const double x = n.separation * (std::cos(E) - e);
		const double z = n.separation * std::sqrt(1.0 - e * e) * std::sin(E);
		const double inclination = glm::radians((double)n.inclination);
		const glm::dvec3 separation(x, z * std::sin(inclination), z * std::cos(inclination));

		// Each about their centre of mass
		const double firstMass = NodeMass(n.first), secondMass = NodeMass(n.second);
		const double total = firstMass + secondMass;
		PlaceNode(n.first, centre - separation * (secondMass / total));
		PlaceNode(n.second, centre + separation * (firstMass / total));

		// Two bodies stretch each other along the line between them
		const StarSystemNode& first = system.nodes[(size_t)n.first];
		const StarSystemNode& second = system.nodes[(size_t)n.second];
		if (first.body >= 0 && second.body >= 0)
		{
			const double distance = glm::length(separation);
			const glm::vec3 axis(separation / std::max(distance, 1e-9));
			auto Stretch = [&](const StellarBody& body, const double otherMass)
			{
				const double ratio = body.radius / std::max(distance, 1e-9);
				return std::min(1.0f + TIDAL_STRETCH * (float)(otherMass / body.mass * ratio * ratio * ratio), MAX_STRETCH);
			};
			stretches[(size_t)first.body] = glm::vec4(axis, Stretch(system.bodies[(size_t)first.body], secondMass));
			stretches[(size_t)second.body] = glm::vec4(axis, Stretch(system.bodies[(size_t)second.body], firstMass));
		}
	}

	void StarSystemRenderer::Update(const double deltaTime)
	{
		days += deltaTime * (double)daysPerSecond;
		seconds += deltaTime;
		granuleAges.resize(system.bodies.size(), 0.0);
		for (size_t i = 0; i < system.bodies.size(); i++)
		{
			const StellarBody& body = system.bodies[i];
			if (body.granulation <= 0.0f)
				continue;
			const double lifetime = SOLAR_GRANULE_LIFETIME * SOLAR_GRANULES * body.radius / body.granulation;
			// (Kept small, so it stays precise in a float)
			granuleAges[i] = std::fmod(granuleAges[i] + deltaTime * std::min((double)daysPerSecond / lifetime, MAX_GRANULE_RATE), 1000.0);
		}
		nodePositions.assign(system.nodes.size(), glm::dvec3(0.0));
		stretches.assign(system.bodies.size(), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
		if (!system.nodes.empty())
			PlaceNode(system.root, glm::dvec3(0.0));
	}

	glm::dvec3 StarSystemRenderer::GetNodePosition(const size_t node) const
	{
		return node < nodePositions.size() ? nodePositions[node] : glm::dvec3(0.0);
	}

	float StarSystemRenderer::GetNodeRadius(const size_t node) const
	{
		if (node >= system.nodes.size() || system.nodes[node].body < 0)
			return 0.0f;
		const size_t body = (size_t)system.nodes[node].body;
		return system.bodies[body].radius * stretches[body].w;
	}

	void StarSystemRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const StarSystemView& view)
	{
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), 1.0f);

		StarSystemUniforms uniforms{};
		const size_t count = std::min(system.bodies.size(), MAX_BODIES);
		// The star field stays as bright as the exposure changes from system to system
		uniforms.info = glm::vec4((float)count, view.pixelAngle, starFieldBrightness / std::max(brightness, 1e-6f), surfaceDetail);
		for (size_t node = 0; node < system.nodes.size(); node++)
		{
			const int index = system.nodes[node].body;
			if (index < 0 || (size_t)index >= count)
				continue;
			const size_t i = (size_t)index;
			const StellarBody& body = system.bodies[i];
			const double rotation = std::fmod(TAU * days / std::max((double)body.rotationPeriod, 1e-6), TAU);
			uniforms.position[i] = glm::vec4(glm::vec3(nodePositions[node] - view.origin), body.radius);
			uniforms.surface[i] = glm::vec4(body.temperature, (float)body.kind, body.granulation, body.spots);
			uniforms.stretch[i] = stretches[i];
			uniforms.look[i] = glm::vec4((float)granuleAges[i], body.glowSize, body.glowStrength, (float)rotation);
			uniforms.tint[i] = glm::vec4(body.tint, BlackbodyLuminance(body.temperature));

			// Flares: one at a time, at a random place, brightening quickly and fading
			if (body.flareRate > 0.0f)
			{
				const double interval = 60.0 / body.flareRate;
				const double cycle = std::floor(seconds / interval);
				const double age = seconds - cycle * interval;
				const float strength = (float)(age < FLARE_RISE ? age / FLARE_RISE : std::exp(-(age - FLARE_RISE) / FLARE_FADE));
				const uint32_t seed = (uint32_t)cycle * 3u + (uint32_t)i * 7919u;
				const float z = Random(seed) * 1.4f - 0.7f; // At low and middle latitudes
				const float angle = Random(seed + 1u) * (float)TAU;
				const float r = std::sqrt(1.0f - z * z);
				uniforms.flare[i] = glm::vec4(r * std::cos(angle), z, r * std::sin(angle), FLARE_BRIGHTNESS * strength);
			}
		}
		uniforms.display = glm::vec4(pointGain, 0.0f, 0.0f, 0.0f);
		uniforms.blackbody = BlackbodyTable();
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*tracePipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *systemGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		compositor.Composite(cmd, target, brightness, bloom);
	}
}
