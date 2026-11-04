#pragma once
#include <string_view>

#include "Dahlia/Graphics/ShaderModule.h"
#include "Dahlia/Utilities/Memory.h"

namespace SV::Shaders
{
	// Create a shader module from the SPIR-V embedded at build time, by its file name in Shaders/
	// (e.g. "Planet.frag", "Fullscreen.vert"). The stage is taken from the extension.
	sRef<DH::ShaderModule> Create(std::string_view name);
}
