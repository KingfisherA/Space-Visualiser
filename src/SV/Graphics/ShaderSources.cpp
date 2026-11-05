#include "ShaderSources.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>

// Each .inc is generated from Shaders/ by dahlia_compile_shaders (see Projects/cmake/SV.cmake)
namespace
{
	const uint32_t kGridVert[] =
#include "Grid.vert.inc"
		;
	const uint32_t kGridFrag[] =
#include "Grid.frag.inc"
		;
	const uint32_t kStarVert[] =
#include "Star.vert.inc"
		;
	const uint32_t kStarFrag[] =
#include "Star.frag.inc"
		;
	const uint32_t kCoronaVert[] =
#include "Corona.vert.inc"
		;
	const uint32_t kCoronaFrag[] =
#include "Corona.frag.inc"
		;
	const uint32_t kFlareVert[] =
#include "Flare.vert.inc"
		;
	const uint32_t kFlareFrag[] =
#include "Flare.frag.inc"
		;
	const uint32_t kRingsVert[] =
#include "Rings.vert.inc"
		;
	const uint32_t kRingsFrag[] =
#include "Rings.frag.inc"
		;
	const uint32_t kPlanetVert[] =
#include "Planet.vert.inc"
		;
	const uint32_t kPlanetFrag[] =
#include "Planet.frag.inc"
		;
	const uint32_t kFullscreenVert[] =
#include "Fullscreen.vert.inc"
		;
	const uint32_t kTransmittanceFrag[] =
#include "Transmittance.frag.inc"
		;
	const uint32_t kDirectIrradianceFrag[] =
#include "DirectIrradiance.frag.inc"
		;
	const uint32_t kSingleScatteringFrag[] =
#include "SingleScattering.frag.inc"
		;
	const uint32_t kScatteringDensityFrag[] =
#include "ScatteringDensity.frag.inc"
		;
	const uint32_t kIndirectIrradianceFrag[] =
#include "IndirectIrradiance.frag.inc"
		;
	const uint32_t kMultipleScatteringFrag[] =
#include "MultipleScattering.frag.inc"
		;

	const std::unordered_map<std::string_view, std::span<const uint32_t>> kShaders = {
		{ "Grid.vert", kGridVert },
		{ "Grid.frag", kGridFrag },
		{ "Star.vert", kStarVert },
		{ "Star.frag", kStarFrag },
		{ "Corona.vert", kCoronaVert },
		{ "Corona.frag", kCoronaFrag },
		{ "Flare.vert", kFlareVert },
		{ "Flare.frag", kFlareFrag },
		{ "Rings.vert", kRingsVert },
		{ "Rings.frag", kRingsFrag },
		{ "Planet.vert", kPlanetVert },
		{ "Planet.frag", kPlanetFrag },
		{ "Fullscreen.vert", kFullscreenVert },
		{ "Transmittance.frag", kTransmittanceFrag },
		{ "DirectIrradiance.frag", kDirectIrradianceFrag },
		{ "SingleScattering.frag", kSingleScatteringFrag },
		{ "ScatteringDensity.frag", kScatteringDensityFrag },
		{ "IndirectIrradiance.frag", kIndirectIrradianceFrag },
		{ "MultipleScattering.frag", kMultipleScatteringFrag },
	};
}

namespace SV::Shaders
{
	sRef<DH::ShaderModule> Create(const std::string_view name)
	{
		const auto it = kShaders.find(name);
		if (it == kShaders.end())
			throw std::invalid_argument("Unknown shader: " + std::string(name));

		const DH::ShaderStage stage = name.ends_with(".vert") ? DH::ShaderStage::Vertex : DH::ShaderStage::Fragment;
		return DH::ShaderModule::Create(stage, it->second);
	}
}
