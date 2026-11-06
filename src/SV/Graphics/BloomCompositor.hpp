#pragma once
#include <array>
#include <cstddef>

#include "Dahlia/Graphics/GraphicsTypes.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/SceneContext.hpp"

namespace DH
{
	class CommandList;
	class Framebuffer;
	class GraphicsPipeline;
}

namespace SV
{
	// The last stage of the test scenes: they render HDR radiance into its scene target, which it
	// blooms (downsampled and blurred at three scales, each wider) and tone maps into a viewport
	// framebuffer
	class BloomCompositor
	{
	public:
		explicit BloomCompositor(const SceneContext& context);
		~BloomCompositor();

		// The attachments of the scene target, for the pipelines drawing into it
		[[nodiscard]] static DH::RenderTargetLayout GetSceneLayout();

		// The scene target for a viewport of this size, at `scale` (0.25 to 1) of its resolution.
		// It (and the bloom) is recreated when either changes.
		[[nodiscard]] DH::Framebuffer& GetSceneTarget(size_t width, size_t height, float scale);

		// Outside a render pass, after the scene target has been rendered; ends with `target`
		// rendered. `bloom` is the bloom's brightness relative to the scene's.
		void Composite(DH::CommandList& cmd, DH::Framebuffer& target, float exposure, float bloom);

	private:
		static constexpr size_t BLOOM_LEVELS = 3;

		// A level of the bloom: blurred across into `across`, then down back into `result`
		struct BloomLevel
		{
			sRef<DH::Framebuffer> result;
			sRef<DH::Framebuffer> across;
			sRef<DH::BindGroup> resultGroup;
			sRef<DH::BindGroup> acrossGroup;
		};

		SceneContext context;

		sRef<DH::Framebuffer> sceneTarget;
		size_t targetWidth = 0;
		size_t targetHeight = 0;
		float targetScale = 0.0f;
		sRef<DH::BindGroupLayout> textureLayout;
		sRef<DH::BindGroup> sceneGroup;
		std::array<BloomLevel, BLOOM_LEVELS> bloomLevels;
		sRef<DH::BindGroupLayout> compositeLayout;
		sRef<DH::BindGroup> compositeGroup;

		sRef<DH::GraphicsPipeline> downsamplePipeline;
		sRef<DH::GraphicsPipeline> blurPipeline;
		sRef<DH::GraphicsPipeline> compositePipeline;
	};
}
