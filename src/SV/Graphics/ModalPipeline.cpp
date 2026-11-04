#include "ModalPipeline.hpp"

namespace SV
{
	ModalPipeline::ModalPipeline(const DH::GraphicsPipelineDesc& descIn) : desc(descIn)
	{
		// Create the solid pipeline up front so description errors surface at start up
		Get(desc.raster.polygonMode);
	}

	const DH::GraphicsPipeline& ModalPipeline::Get(const DH::PolygonMode mode)
	{
		sRef<DH::GraphicsPipeline>& pipeline = pipelines[(size_t)mode];
		if (pipeline == nullptr)
		{
			DH::GraphicsPipelineDesc modeDesc = desc;
			modeDesc.raster.polygonMode = mode;
			if (mode != DH::PolygonMode::Solid)
				modeDesc.raster.cullMode = DH::CullMode::None;
			pipeline = DH::GraphicsPipeline::Create(modeDesc);
		}
		return *pipeline;
	}
}
