#pragma once
#include <array>

#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Utilities/Memory.h"

namespace SV
{
	// A pipeline that can be drawn solid, as wireframe or as points (the 1/2/3 keys). Polygon mode
	// is baked into Dahlia pipelines, so each mode's pipeline is created the first time it is used.
	// Wireframe and point modes disable face culling, as Space Visualiser always has.
	class ModalPipeline
	{
	public:
		ModalPipeline() = default;
		explicit ModalPipeline(const DH::GraphicsPipelineDesc& desc);

		const DH::GraphicsPipeline& Get(DH::PolygonMode mode = DH::PolygonMode::Solid);

	private:
		DH::GraphicsPipelineDesc desc;
		std::array<sRef<DH::GraphicsPipeline>, 3> pipelines;
	};
}
