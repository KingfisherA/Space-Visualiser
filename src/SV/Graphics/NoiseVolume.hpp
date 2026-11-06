#pragma once
#include <cstdint>

#include "Dahlia/Utilities/Memory.h"

namespace DH
{
	class Sampler;
	class Texture3D;
}

namespace SV
{
	// The noise volume's size in voxels (a cube)
	constexpr uint32_t NOISE_VOLUME_SIZE = 128;

	// Four independent channels of tileable gradient noise fBm, each equalised to spread evenly over
	// [0, 1] (so thresholds in the shaders cover predictable fractions), RGBA8. It is made once and
	// shared while any renderer holds it.
	[[nodiscard]] sRef<DH::Texture3D> GetNoiseVolume();
	// Trilinear, wrapping (the volume tiles)
	[[nodiscard]] sRef<DH::Sampler> CreateNoiseSampler();
}
