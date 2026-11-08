#include "CompactObject.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "SV/Graphics/ShaderSources.hpp"
#include "Star.hpp"

namespace SV
{
	namespace
	{
		// Mirrors CompactObjectUniforms in Shaders/CompactObject.frag (std140)
		struct CompactObjectUniforms
		{
			glm::vec4 centre;   // xyz = the object's centre (render coordinates), w = time
			glm::vec4 disc;     // x = inner radius, y = outer radius (0 for no disc), z = opacity, w = brightness
			glm::vec4 discLook; // x = Doppler beaming, y = gravitational redshift, z = peak temperature, w = turbulence
			glm::vec4 jets;     // x = brightness (0 for none), y = speed (c), z = tan(half opening angle), w = length
			glm::vec4 sky;      // x = star brightness, y = sky brightness, z = pixel angle, w = escape radius
			glm::vec4 star;     // x = radius (0 for a black hole), y = brightness, z = temperature, w = cos(hot spot radius)
			glm::vec4 magnetic; // xyz = the magnetic axis, w = hot spot brightness
			glm::vec4 beams;    // x = brightness (0 for none), y = half width (radians), z = length, w = magnetosphere
			glm::vec4 surface;  // xyz = a unit vector across the magnetic axis, turning with the star, w = surface detail
			glm::vec4 glare;    // x = brightness, y = size
			glm::vec4 skyMap;   // x = the angle across a texel of the sky map's first level (0 until it is baked), y = its last level
			std::array<glm::vec4, BLACKBODY_TABLE_SIZE> blackbody;
		};

		// Rays that have left this far (and are heading away) see the sky. It takes in the disc and
		// the jets or beams, and is far enough out that what bending remains is slight.
		constexpr float MIN_ESCAPE_RADIUS = 40.0f;
		constexpr float ESCAPE_MARGIN = 1.5f;

		// The time is kept within this, a multiple of the disc's flow period (FLOW_PERIOD in
		// Shaders/CompactObject.frag), so it stays precise in a float
		constexpr double TIME_WRAP = 30.0 * 200.0;
		constexpr double TAU = 2.0 * std::numbers::pi;

		// The sky map's size (texels; its texels are about as fine as a 1080p viewport's pixels,
		// and its mip levels with it take 85 MB) and how many of its texels are baked a frame
		constexpr uint32_t SKY_MAP_SIZE = 4096;
		constexpr size_t SKY_MAP_TEXELS_PER_FRAME = 4096 * 1024;

		// Mirrors SkyBakePushConstants in Shaders/SkyBake.frag
		struct SkyBakePushConstants
		{
			glm::vec4 level; // x = this level's size (texels), y = the angle across one of its texels
		};

		float SkyMapTexelAngle(const uint32_t size)
		{
			// Its texels share the sphere's 4 pi steradians (nearly evenly)
			return 2.0f * std::sqrt(std::numbers::pi_v<float>) / (float)size;
		}
	}

	SkyMap::SkyMap()
	{
		// 8 bits a channel, so the map has mip levels on every backend
		texture = DH::Texture2D::Create({ SKY_MAP_SIZE, SKY_MAP_SIZE }, nullptr, DH::Format::RGBA8, DH::TextureUsage::SampledRenderTarget);
		DH::SamplerDesc samplerDesc;
		samplerDesc.mipmapMode = DH::MipmapMode::Linear;
		sampler = DH::Sampler::Create(samplerDesc);

		// Each level is baked from the sky itself, in strips of rows
		for (uint32_t level = 0; level < texture->GetMipLevelCount(); level++)
		{
			DH::FramebufferSpecification spec;
			spec.attachments = { DH::FramebufferAttachment(texture, level) };
			levels.push_back(DH::Framebuffer::Create(spec));

			const uint32_t size = std::max(SKY_MAP_SIZE >> level, 1u);
			const uint32_t rowsPerStrip = (uint32_t)std::max<size_t>(SKY_MAP_TEXELS_PER_FRAME / size, 1);
			for (uint32_t row = 0; row < size; row += rowsPerStrip)
				work.push_back({ level, row, std::min(rowsPerStrip, size - row) });
		}

		DH::GraphicsPipelineDesc bakeDesc;
		bakeDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle
		bakeDesc.fragmentShader = Shaders::Create("SkyBake.frag");
		bakeDesc.depth = { false, false, DH::CompareOp::Always };
		bakeDesc.blend = DH::BlendPreset::None;
		bakeDesc.targetLayout = levels[0]->GetLayout();
		bakeDesc.pushConstantSize = sizeof(SkyBakePushConstants);
		bakePipeline = DH::GraphicsPipeline::Create(bakeDesc);
	}

	SkyMap::~SkyMap() = default;

	sRef<SkyMap> SkyMap::Get()
	{
		static std::weak_ptr<SkyMap> shared;
		sRef<SkyMap> map = shared.lock();
		if (map == nullptr)
		{
			map = std::make_shared<SkyMap>();
			shared = map;
		}
		return map;
	}

	void SkyMap::Bake(DH::CommandList& cmd)
	{
		size_t texels = 0;
		while (next < work.size() && texels < SKY_MAP_TEXELS_PER_FRAME)
		{
			const Strip& strip = work[next++];
			const uint32_t size = std::max(SKY_MAP_SIZE >> strip.level, 1u);
			texels += (size_t)size * strip.rows;

			// The rest of the level is kept (it is baked by the other strips)
			DH::RenderPassDesc pass;
			pass.target = levels[strip.level].get();
			pass.colourLoadOp = strip.firstRow == 0 ? DH::LoadOp::DontCare : DH::LoadOp::Load;
			pass.depthLoadOp = DH::LoadOp::DontCare;
			cmd.BeginRenderPass(pass);
			cmd.SetScissor(glm::ivec4(0, (int)strip.firstRow, (int)size, (int)strip.rows));
			cmd.BindPipeline(*bakePipeline);
			cmd.PushConstants(SkyBakePushConstants{ glm::vec4((float)size, SkyMapTexelAngle(size), 0.0f, 0.0f) });
			cmd.Draw(3);
			cmd.EndRenderPass();
		}
	}

	float SkyMap::GetTexelAngle() const
	{
		return SkyMapTexelAngle(SKY_MAP_SIZE);
	}

	uint32_t SkyMap::GetLevelCount() const
	{
		return (uint32_t)levels.size();
	}

	CompactObjectParameters GetBlackHolePreset()
	{
		return CompactObjectParameters{};
	}

	CompactObjectParameters GetPulsarPreset()
	{
		CompactObjectParameters p;
		p.type = CompactObjectType::NeutronStar;
		// The disc is held off by the star's magnetic field, and is cooler and fainter so the star
		// and its beams stand out
		p.discInnerRadius = 9.0f;
		p.discOuterRadius = 28.0f;
		p.discTemperature = 6000.0f;
		p.discBrightness = 1.2f;
		p.discOpacity = 0.7f;
		p.timeScale = 20.0f;
		p.showJets = false;
		p.starBrightness = 4.0f;
		p.beamBrightness = 1.5f;
		return p;
	}

	CompactObjectRenderer::CompactObjectRenderer(const SceneContext& contextIn, const CompactObjectParameters& parametersIn) :
		parameters(parametersIn),
		context(contextIn),
		compositor(contextIn),
		skyMap(SkyMap::Get())
	{
		const CompactObjectUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(CompactObjectUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);

		// Set 1 of the trace: the object's parameters and the sky map
		objectLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler, DH::ShaderStage::Fragment } });
		objectGroup = DH::BindGroup::Create(objectLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, skyMap->texture, skyMap->sampler) });

		// The trace: written over the whole scene target
		DH::GraphicsPipelineDesc traceDesc;
		traceDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle with each pixel's NDC
		traceDesc.fragmentShader = Shaders::Create("CompactObject.frag");
		traceDesc.depth = { false, false, DH::CompareOp::Always };
		traceDesc.blend = DH::BlendPreset::None;
		traceDesc.targetLayout = BloomCompositor::GetSceneLayout();
		traceDesc.bindGroupLayouts = { context.frameLayout, objectLayout };
		tracePipeline = DH::GraphicsPipeline::Create(traceDesc);
	}

	CompactObjectRenderer::~CompactObjectRenderer() = default;

	void CompactObjectRenderer::Update(const double deltaTime)
	{
		time = std::fmod(time + deltaTime * (double)parameters.timeScale, TIME_WRAP);
		spinAngle = std::fmod(spinAngle + TAU * deltaTime / std::max((double)parameters.spinPeriod, 1e-3), TAU);
	}

	void CompactObjectRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const CompactObjectView& view)
	{
		skyMap->Bake(cmd);
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), parameters.renderScale);

		const CompactObjectParameters& p = parameters;
		const bool isStar = p.type == CompactObjectType::NeutronStar;
		const float pixelAngle = view.pixelAngle * (float)target.GetHeight() / (float)scene.GetHeight();
		const float jetReach = p.showJets ? 2.0f * p.jetLength : 0.0f;
		const float beamReach = isStar ? 2.0f * p.beamLength : 0.0f;
		// The magnetic axis, tilted from the spin axis (y) and turning about it with the star
		const float tilt = glm::radians(p.magneticTilt);
		const glm::vec3 magneticAxis(std::sin(tilt) * (float)std::cos(spinAngle), std::cos(tilt), std::sin(tilt) * (float)std::sin(spinAngle));
		// With it, the star's own frame: a direction across the magnetic axis, turning with the star
		const glm::vec3 acrossAxis(std::cos(tilt) * (float)std::cos(spinAngle), -std::sin(tilt), std::cos(tilt) * (float)std::sin(spinAngle));

		const CompactObjectUniforms uniforms{
			glm::vec4(view.centre, (float)time),
			glm::vec4(p.discInnerRadius, p.showDisc ? std::max(p.discOuterRadius, p.discInnerRadius) : 0.0f, p.discOpacity, p.discBrightness),
			glm::vec4(p.dopplerBeaming, p.gravitationalRedshift, p.discTemperature, p.discTurbulence),
			glm::vec4(p.showJets ? p.jetBrightness : 0.0f, p.jetSpeed, std::tan(glm::radians(p.jetOpeningAngle)), p.jetLength),
			glm::vec4(p.starFieldBrightness, p.skyBrightness, pixelAngle,
					  ESCAPE_MARGIN * std::max({ MIN_ESCAPE_RADIUS, p.discOuterRadius, jetReach, beamReach })),
			glm::vec4(isStar ? std::max(p.starRadius, 1.05f) : 0.0f, p.starBrightness, p.starTemperature, std::cos(glm::radians(p.hotSpotSize))),
			glm::vec4(magneticAxis, p.hotSpotBrightness),
			glm::vec4(isStar ? p.beamBrightness : 0.0f, glm::radians(std::max(p.beamWidth, 0.1f)), p.beamLength, isStar ? p.magnetosphere : 0.0f),
			glm::vec4(acrossAxis, p.surfaceDetail),
			glm::vec4(p.glareBrightness, std::max(p.glareSize, 0.05f), 0.0f, 0.0f),
			glm::vec4(skyMap->IsReady() ? skyMap->GetTexelAngle() : 0.0f, (float)skyMap->GetLevelCount() - 1.0f, 0.0f, 0.0f),
			BlackbodyTable()
		};
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*tracePipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *objectGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		compositor.Composite(cmd, target, p.brightness, p.bloom, compositeMode, opacity);
	}
}
