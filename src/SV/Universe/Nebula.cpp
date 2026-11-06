#include "Nebula.hpp"

#include <algorithm>
#include <array>
#include <cmath>

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
		// Mirrors NebulaUniforms in Shaders/Nebula.frag (std140)
		struct NebulaUniforms
		{
			glm::vec4 centre; // xyz = the nebula's centre (render coordinates), w = time
			glm::vec4 shape;  // xyz = semi-axes, w = kind (0 supernova remnant, 1 H II region)
			glm::vec4 form;   // x = tilt (radians), y = detail, z = turbulence, w = filament sharpness
			glm::vec4 lines;  // x = hydrogen, y = oxygen, z = nitrogen and sulphur, w = continuum
			glm::vec4 look;   // x = dust, y = filaments, z = star field brightness, w = pixel angle
			glm::vec4 bounds; // x = bounding radius, y = star count
			glm::vec4 light;  // xyz = towards the stars lighting the pillars, w = pillar count
			glm::vec4 hydrogenColour;
			glm::vec4 oxygenColour;
			glm::vec4 sulphurColour;
			std::array<glm::vec4, MAX_NEBULA_PILLARS> pillarBase; // xyz = base, w = radius there
			std::array<glm::vec4, MAX_NEBULA_PILLARS> pillarHead; // xyz = head, w = radius there
			std::array<glm::vec4, MAX_NEBULA_STARS> stars;    // xyz = position, w = brightness
			std::array<glm::vec4, MAX_NEBULA_STARS> starInfo; // x = temperature, y = ionising radius, z = ionising strength
			std::array<glm::vec4, BLACKBODY_TABLE_SIZE> blackbody;
		};

		// The gas is raymarched within a sphere this much larger than its largest semi-axis: a
		// supernova remnant's ends there, but an H II region's cloud goes on behind it
		constexpr float REMNANT_BOUNDS = 1.25f;
		constexpr float HII_REGION_BOUNDS = 1.6f;
		// The time is kept within this, so it stays precise in a float
		constexpr double TIME_WRAP = 1000.0;

		// The emission lines' colours (linear sRGB, luminance about 1), as they are: H-alpha with
		// some H-beta (pink-red), [O III] (green-blue), and [N II] and [S II] (deep red to orange)
		constexpr glm::vec3 HYDROGEN_COLOUR{ 2.4f, 0.35f, 0.55f };
		constexpr glm::vec3 OXYGEN_COLOUR{ 0.15f, 1.15f, 0.95f };
		constexpr glm::vec3 SULPHUR_COLOUR{ 2.6f, 0.5f, 0.12f };
		// And in the Hubble palette: [S II] red, hydrogen green, [O III] blue
		constexpr glm::vec3 HUBBLE_HYDROGEN_COLOUR{ 0.3f, 1.15f, 0.25f };
		constexpr glm::vec3 HUBBLE_OXYGEN_COLOUR{ 0.1f, 0.55f, 2.4f };
		constexpr glm::vec3 HUBBLE_SULPHUR_COLOUR{ 2.8f, 0.3f, 0.1f };

		NebulaPreset Crab()
		{
			// M1, 2 kpc away: 4.4 by 2.9 pc, its long axis from north-west to south-east
			NebulaPreset preset;
			preset.name = "Crab Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::SupernovaRemnant;
			p.size = { 2.2f, 1.45f, 1.5f };
			p.tilt = 35.0f;
			p.hydrogen = 0.8f;
			p.oxygen = 0.7f;
			p.sulphur = 1.2f;
			p.continuum = 1.0f;
			p.dust = 0.4f;
			p.detail = 0.3f;
			p.filaments = 1.0f;
			p.filamentSharpness = 10.0f;
			p.turbulence = 0.5f;
			p.stars = {
				{ "Crab Pulsar", { 0.0f, 0.0f, 0.0f }, 0.8f, 25000.0f },
				{ "", { 0.04f, 0.06f, 0.4f }, 0.6f, 6500.0f }, // The star beside the pulsar (in the line of sight)
				{ "", { 1.3f, -1.6f, 3.0f }, 0.5f, 5200.0f },
				{ "", { -2.1f, 0.9f, 2.0f }, 0.35f, 8000.0f },
				{ "", { 2.7f, 1.4f, -2.0f }, 0.3f, 4200.0f },
			};
			preset.overviewZoom = 9.0f;
			return preset;
		}

		NebulaPreset Orion()
		{
			// M42, 414 pc away: about 7 pc across, around the Trapezium (with M43 to the north)
			NebulaPreset preset;
			preset.name = "Orion Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::HIIRegion;
			p.size = { 4.0f, 3.6f, 2.2f };
			p.tilt = 0.0f;
			p.hydrogen = 1.0f;
			p.oxygen = 1.0f;
			p.sulphur = 0.6f;
			p.continuum = 0.25f;
			p.dust = 1.0f;
			p.brightness = 1.6f;
			p.detail = 0.3f;
			p.turbulence = 0.6f;
			p.stars = {
				// The Trapezium: Theta-1 Orionis C (which ionises most of the nebula), A, B and D
				{ "Trapezium", { 0.0f, 0.0f, 0.05f }, 4.0f, 39000.0f, 1.2f, 1.0f },
				{ "", { 0.03f, 0.035f, 0.08f }, 1.6f, 30000.0f },
				{ "", { -0.035f, 0.025f, 0.0f }, 1.2f, 25000.0f },
				{ "", { 0.02f, -0.04f, 0.04f }, 1.6f, 32000.0f },
				{ "Theta-2 Orionis", { 0.42f, -0.5f, 0.25f }, 2.2f, 34000.0f, 0.4f, 0.15f },
				// NU Orionis, which ionises M43
				{ "M43", { 0.35f, 2.0f, -0.25f }, 2.0f, 30000.0f, 0.55f, 0.35f },
				// Members of the Orion Nebula Cluster
				{ "", { -0.3f, 0.25f, 0.1f }, 0.5f, 5000.0f },
				{ "", { 0.45f, 0.3f, -0.05f }, 0.4f, 4200.0f },
				{ "", { -0.15f, -0.4f, 0.2f }, 0.6f, 9000.0f },
				{ "", { 0.25f, -0.15f, 0.3f }, 0.35f, 3800.0f },
				{ "", { -0.55f, -0.1f, 0.0f }, 0.45f, 6500.0f },
				{ "", { 0.1f, 0.5f, 0.15f }, 0.3f, 4500.0f },
				{ "", { -0.4f, 0.6f, -0.1f }, 0.35f, 7000.0f },
				{ "", { 0.7f, 0.05f, 0.1f }, 0.3f, 5500.0f },
			};
			preset.overviewZoom = 11.0f;
			return preset;
		}

		NebulaPreset PillarsOfCreation()
		{
			// In M16 (the Eagle Nebula), 1.7 kpc away: three pillars, the tallest 1.3 pc, worn away
			// from above by the hot stars of NGC 6611. Shown in the Hubble palette, as in Hubble's
			// image of them.
			NebulaPreset preset;
			preset.name = "Pillars of Creation";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::Pillars;
			p.size = { 1.5f, 1.3f, 1.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 1.0f;
			p.sulphur = 1.0f;
			p.continuum = 0.0f;
			p.dust = 1.0f;
			p.detail = 0.8f;
			p.turbulence = 0.12f;
			p.pillars = {
				{ { -0.6f, -1.15f, 0.0f }, { -0.38f, 0.42f, 0.05f }, 0.27f, 0.15f },
				{ { 0.08f, -1.15f, -0.1f }, { 0.14f, -0.12f, -0.05f }, 0.2f, 0.13f },
				{ { 0.62f, -1.15f, -0.2f }, { 0.56f, -0.46f, -0.12f }, 0.16f, 0.1f },
			};
			p.lightDirection = glm::normalize(glm::vec3(0.1f, 1.0f, 0.35f));
			p.hubblePalette = true;
			p.stars = {
				{ "", { 0.9f, 0.7f, 0.6f }, 0.6f, 9000.0f },
				{ "", { -1.0f, -0.3f, 0.5f }, 0.4f, 5000.0f },
				{ "", { 0.3f, 0.9f, 0.3f }, 0.35f, 15000.0f },
				{ "", { -0.2f, -0.8f, 0.7f }, 0.3f, 4200.0f },
				{ "", { 1.2f, -0.6f, 0.2f }, 0.45f, 7000.0f },
			};
			p.brightness = 1.4f;
			preset.overviewZoom = 3.4f;
			return preset;
		}
	}

	const NebulaPreset& GetNebulaPreset(const NebulaType type)
	{
		static const std::array<NebulaPreset, (size_t)NebulaType::Count> presets = { Crab(), Orion(), PillarsOfCreation() };
		return presets[std::min((size_t)type, presets.size() - 1)];
	}

	NebulaRenderer::NebulaRenderer(const SceneContext& contextIn, const NebulaParameters& parametersIn) :
		parameters(parametersIn),
		context(contextIn),
		compositor(contextIn)
	{
		const NebulaUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(NebulaUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);
		noiseTexture = GetNoiseVolume();
		noiseSampler = CreateNoiseSampler();

		// Set 1 of the march: the nebula's parameters and the noise
		nebulaLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler3D, DH::ShaderStage::Fragment } });
		nebulaGroup = DH::BindGroup::Create(nebulaLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, noiseTexture, noiseSampler) });

		DH::GraphicsPipelineDesc marchDesc;
		marchDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle with each pixel's NDC
		marchDesc.fragmentShader = Shaders::Create("Nebula.frag");
		marchDesc.depth = { false, false, DH::CompareOp::Always };
		marchDesc.blend = DH::BlendPreset::None;
		marchDesc.targetLayout = BloomCompositor::GetSceneLayout();
		marchDesc.bindGroupLayouts = { context.frameLayout, nebulaLayout };
		marchPipeline = DH::GraphicsPipeline::Create(marchDesc);
	}

	NebulaRenderer::~NebulaRenderer() = default;

	void NebulaRenderer::Update(const double deltaTime)
	{
		time = std::fmod(time + deltaTime, TIME_WRAP);
	}

	void NebulaRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const NebulaView& view)
	{
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), parameters.renderScale);

		const NebulaParameters& p = parameters;
		const float pixelAngle = view.pixelAngle * (float)target.GetHeight() / (float)scene.GetHeight();
		const size_t starCount = std::min(p.stars.size(), MAX_NEBULA_STARS);
		const size_t pillarCount = std::min(p.pillars.size(), MAX_NEBULA_PILLARS);
		NebulaUniforms uniforms{
			glm::vec4(view.centre, (float)time),
			glm::vec4(p.size, (float)p.kind),
			glm::vec4(glm::radians(p.tilt), p.detail, p.turbulence, p.filamentSharpness),
			glm::vec4(p.hydrogen, p.oxygen, p.sulphur, p.continuum),
			glm::vec4(p.dust, p.filaments, p.starFieldBrightness, pixelAngle),
			glm::vec4((p.kind == NebulaKind::SupernovaRemnant ? REMNANT_BOUNDS : HII_REGION_BOUNDS) * std::max({ p.size.x, p.size.y, p.size.z }),
					  (float)starCount, 0.0f, 0.0f),
			glm::vec4(glm::normalize(p.lightDirection), (float)pillarCount),
			glm::vec4(p.hubblePalette ? HUBBLE_HYDROGEN_COLOUR : HYDROGEN_COLOUR, 0.0f),
			glm::vec4(p.hubblePalette ? HUBBLE_OXYGEN_COLOUR : OXYGEN_COLOUR, 0.0f),
			glm::vec4(p.hubblePalette ? HUBBLE_SULPHUR_COLOUR : SULPHUR_COLOUR, 0.0f),
			{},
			{},
			{},
			{},
			BlackbodyTable()
		};
		for (size_t i = 0; i < pillarCount; i++)
		{
			const NebulaPillar& pillar = p.pillars[i];
			uniforms.pillarBase[i] = glm::vec4(pillar.base, pillar.baseRadius);
			uniforms.pillarHead[i] = glm::vec4(pillar.head, pillar.headRadius);
		}
		for (size_t i = 0; i < starCount; i++)
		{
			const NebulaStar& star = p.stars[i];
			uniforms.stars[i] = glm::vec4(star.position, star.brightness);
			uniforms.starInfo[i] = glm::vec4(star.temperature, star.ionisingRadius, star.ionisingStrength, 0.0f);
		}
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*marchPipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *nebulaGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		compositor.Composite(cmd, target, p.brightness, p.bloom);
	}
}
