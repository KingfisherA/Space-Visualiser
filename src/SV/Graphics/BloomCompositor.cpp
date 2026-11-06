#include "BloomCompositor.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Platform/GPUContext.h"
#include "ShaderSources.hpp"

namespace SV
{
	namespace
	{
		// Mirrors the push constants of Shaders/BloomDownsample.frag and BloomBlur.frag
		struct BloomPushConstants
		{
			glm::vec4 params; // xy = the offset between taps (source UV), zw = 1 / the target's size
		};

		// Mirrors the push constants of Shaders/BloomComposite.frag
		struct CompositePushConstants
		{
			glm::vec4 params; // x = exposure, y = bloom, zw = 1 / the target's size
			glm::vec4 blend;  // x = alpha, y = colour scale
		};

		constexpr DH::Format HDR_FORMAT = DH::Format::RGBA16F;
		// Each bloom level is a quarter of the size (across and down) of the one above
		constexpr size_t BLOOM_REDUCTION = 4;
		// Each level is blurred (across then down) this many times, for a wider, softer glow. (The
		// blur's taps sit between pairs of texels to sample both, so spreading them out would skip
		// texels and leave a grid in the glow.)
		constexpr int BLUR_PASSES = 2;
	}

	DH::RenderTargetLayout BloomCompositor::GetSceneLayout()
	{
		DH::RenderTargetLayout layout;
		layout.colourFormats = { HDR_FORMAT };
		return layout;
	}

	BloomCompositor::BloomCompositor(const SceneContext& contextIn) :
		context(contextIn)
	{
		// Set 0 of the bloom passes: their source
		textureLayout = DH::BindGroupLayout::Create({ { 0, DH::ShaderResourceType::CombinedTextureSampler, DH::ShaderStage::Fragment } });
		// Set 0 of the composite: the scene and the bloom levels
		std::vector<DH::BindGroupLayoutEntry> compositeEntries;
		for (uint32_t binding = 0; binding <= BLOOM_LEVELS; binding++)
			compositeEntries.push_back({ binding, DH::ShaderResourceType::CombinedTextureSampler, DH::ShaderStage::Fragment });
		compositeLayout = DH::BindGroupLayout::Create(compositeEntries);

		DH::GraphicsPipelineDesc bloomDesc;
		bloomDesc.vertexShader = Shaders::Create("Fullscreen.vert");
		bloomDesc.depth = { false, false, DH::CompareOp::Always };
		bloomDesc.blend = DH::BlendPreset::None;
		bloomDesc.targetLayout = GetSceneLayout();
		bloomDesc.bindGroupLayouts = { textureLayout };
		bloomDesc.pushConstantSize = sizeof(BloomPushConstants);
		bloomDesc.fragmentShader = Shaders::Create("BloomDownsample.frag");
		downsamplePipeline = DH::GraphicsPipeline::Create(bloomDesc);
		bloomDesc.fragmentShader = Shaders::Create("BloomBlur.frag");
		blurPipeline = DH::GraphicsPipeline::Create(bloomDesc);

		DH::GraphicsPipelineDesc compositeDesc;
		compositeDesc.vertexShader = Shaders::Create("Fullscreen.vert");
		compositeDesc.fragmentShader = Shaders::Create("BloomComposite.frag");
		compositeDesc.depth = { false, false, DH::CompareOp::Always };
		compositeDesc.targetLayout = context.targetLayout;
		compositeDesc.bindGroupLayouts = { compositeLayout };
		compositeDesc.pushConstantSize = sizeof(CompositePushConstants);
		for (const auto& [mode, blend] : { std::pair{ CompositeMode::Replace, DH::BlendPreset::None },
										   std::pair{ CompositeMode::Blend, DH::BlendPreset::Alpha },
										   std::pair{ CompositeMode::Add, DH::BlendPreset::AdditiveOne } })
		{
			compositeDesc.blend = blend;
			compositePipelines[(size_t)mode] = DH::GraphicsPipeline::Create(compositeDesc);
		}
	}

	BloomCompositor::~BloomCompositor() = default;

	DH::Framebuffer& BloomCompositor::GetSceneTarget(const size_t width, const size_t height, const float scaleIn)
	{
		const float scale = std::clamp(scaleIn, 0.25f, 1.0f);
		if (sceneTarget != nullptr && targetWidth == width && targetHeight == height && targetScale == scale)
			return *sceneTarget;
		// The previous targets may still be in use
		if (sceneTarget != nullptr)
			DH::GPUContext::Get()->WaitIdle();
		targetWidth = width;
		targetHeight = height;
		targetScale = scale;

		auto CreateTarget = [](const size_t w, const size_t h)
		{
			return DH::Framebuffer::Create(DH::FramebufferSpecification{ std::max<size_t>(w, 1), std::max<size_t>(h, 1), 1,
																		 { DH::FramebufferAttachment(HDR_FORMAT) } });
		};
		auto CreateGroup = [this](const sRef<DH::Framebuffer>& source)
		{
			return DH::BindGroup::Create(textureLayout, { DH::BindGroupEntry::TextureSampler(0, source->GetAttachmentTexture(0), context.clampSampler) });
		};

		sceneTarget = CreateTarget((size_t)std::ceil((float)width * scale), (size_t)std::ceil((float)height * scale));
		sceneGroup = CreateGroup(sceneTarget);
		size_t levelWidth = sceneTarget->GetWidth(), levelHeight = sceneTarget->GetHeight();
		for (BloomLevel& level : bloomLevels)
		{
			levelWidth = std::max<size_t>((levelWidth + BLOOM_REDUCTION - 1) / BLOOM_REDUCTION, 1);
			levelHeight = std::max<size_t>((levelHeight + BLOOM_REDUCTION - 1) / BLOOM_REDUCTION, 1);
			level.result = CreateTarget(levelWidth, levelHeight);
			level.across = CreateTarget(levelWidth, levelHeight);
			level.resultGroup = CreateGroup(level.result);
			level.acrossGroup = CreateGroup(level.across);
		}

		std::vector<DH::BindGroupEntry> compositeEntries = { DH::BindGroupEntry::TextureSampler(0, sceneTarget->GetAttachmentTexture(0), context.clampSampler) };
		for (size_t i = 0; i < BLOOM_LEVELS; i++)
			compositeEntries.push_back(DH::BindGroupEntry::TextureSampler((uint32_t)i + 1, bloomLevels[i].result->GetAttachmentTexture(0), context.clampSampler));
		compositeGroup = DH::BindGroup::Create(compositeLayout, compositeEntries);
		return *sceneTarget;
	}

	void BloomCompositor::Composite(DH::CommandList& cmd, DH::Framebuffer& target, const float exposure, const float bloom,
									const CompositeMode mode, const float opacity)
	{
		DH::RenderPassDesc pass;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;

		// The bloom: each level from the one above, blurred across then down
		auto Pass = [&](DH::Framebuffer& passTarget, DH::GraphicsPipeline& pipeline, DH::BindGroup& source, const glm::vec2 offset)
		{
			pass.target = &passTarget;
			cmd.BeginRenderPass(pass);
			cmd.BindPipeline(pipeline);
			cmd.BindGroup(0, source);
			cmd.PushConstants(BloomPushConstants{ glm::vec4(offset, 1.0f / (float)passTarget.GetWidth(), 1.0f / (float)passTarget.GetHeight()) });
			cmd.Draw(3);
			cmd.EndRenderPass();
		};
		DH::Framebuffer* source = sceneTarget.get();
		DH::BindGroup* sourceGroup = sceneGroup.get();
		for (BloomLevel& level : bloomLevels)
		{
			Pass(*level.result, *downsamplePipeline, *sourceGroup, 1.0f / glm::vec2((float)source->GetWidth(), (float)source->GetHeight()));
			const glm::vec2 texel = 1.0f / glm::vec2((float)level.result->GetWidth(), (float)level.result->GetHeight());
			for (int blur = 0; blur < BLUR_PASSES; blur++)
			{
				Pass(*level.across, *blurPipeline, *level.resultGroup, glm::vec2(texel.x, 0.0f));
				Pass(*level.result, *blurPipeline, *level.acrossGroup, glm::vec2(0.0f, texel.y));
			}
			source = level.result.get();
			sourceGroup = level.resultGroup.get();
		}

		// The scene and its bloom, tone mapped into the target
		pass.target = &target;
		pass.clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
		pass.colourLoadOp = mode == CompositeMode::Replace ? DH::LoadOp::Clear : DH::LoadOp::Load;
		pass.depthLoadOp = DH::LoadOp::Clear;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*compositePipelines[(size_t)mode]);
		cmd.BindGroup(0, *compositeGroup);
		const glm::vec4 output = mode == CompositeMode::Blend ? glm::vec4(opacity, 1.0f, 0.0f, 0.0f)
								 : mode == CompositeMode::Add ? glm::vec4(1.0f, opacity, 0.0f, 0.0f)
															  : glm::vec4(1.0f);
		cmd.PushConstants(CompositePushConstants{ glm::vec4(exposure, bloom, 1.0f / (float)target.GetWidth(), 1.0f / (float)target.GetHeight()), output });
		cmd.Draw(3);
		cmd.EndRenderPass();
	}
}
