#include "NoiseVolume.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Graphics/Texture.h"

namespace SV
{
	namespace
	{
		constexpr float PI = std::numbers::pi_v<float>;

		// The periods of the octaves (in lattice cells across the volume, powers of two) and how
		// much each is weaker than the last
		constexpr uint32_t NOISE_SIZE = NOISE_VOLUME_SIZE;
		constexpr std::array<uint32_t, 5> NOISE_PERIODS = { 4, 8, 16, 32, 64 };
		constexpr float NOISE_PERSISTENCE = 0.5f;

	// Four independent channels of tileable gradient noise fBm, each equalised to spread evenly
	// over [0, 1] (so thresholds in the shaders cover predictable fractions), RGBA8, x fastest
	std::vector<unsigned char> CreateNoiseData()
	{
		constexpr uint32_t N = NOISE_SIZE;
		constexpr size_t VOXELS = (size_t)N * N * N;
		std::vector<unsigned char> data(VOXELS * 4, 0);

		auto Mix = [](uint32_t h)
		{
			h ^= h >> 16;
			h *= 0x7feb352du;
			h ^= h >> 15;
			h *= 0x846ca68bu;
			h ^= h >> 16;
			return h;
		};

		auto CreateChannel = [&](const uint32_t channel)
		{
			std::vector<float> values(VOXELS, 0.0f);
			std::vector<glm::vec3> gradients;
			float amplitude = 1.0f;
			for (size_t octave = 0; octave < NOISE_PERIODS.size(); octave++)
			{
				// A random unit gradient at each lattice point, wrapping every `period` cells
				const uint32_t period = NOISE_PERIODS[octave];
				const uint32_t mask = period - 1;
				gradients.resize((size_t)period * period * period);
				const uint32_t seed = Mix(channel * 64u + (uint32_t)octave + 1u);
				for (size_t i = 0; i < gradients.size(); i++)
				{
					const uint32_t h = Mix((uint32_t)i ^ seed);
					const float z = (float)(h & 0xffffu) / 65535.0f * 2.0f - 1.0f;
					const float angle = (float)(h >> 16) / 65535.0f * 2.0f * PI;
					const float radius = std::sqrt(std::max(1.0f - z * z, 0.0f));
					gradients[i] = { radius * std::cos(angle), radius * std::sin(angle), z };
				}
				auto Gradient = [&](const uint32_t x, const uint32_t y, const uint32_t z)
				{
					return gradients[((size_t)(z & mask) * period + (y & mask)) * period + (x & mask)];
				};

				const float scale = (float)period / (float)N;
				size_t index = 0;
				for (uint32_t z = 0; z < N; z++)
				{
					for (uint32_t y = 0; y < N; y++)
					{
						for (uint32_t x = 0; x < N; x++)
						{
							const glm::vec3 position = glm::vec3((float)x, (float)y, (float)z) * scale;
							const glm::vec3 cellPosition = glm::floor(position);
							const glm::uvec3 cell(cellPosition);
							const glm::vec3 f = position - cellPosition;
							const glm::vec3 w = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
							float corners[8];
							for (uint32_t corner = 0; corner < 8; corner++)
							{
								const glm::uvec3 offset(corner & 1u, (corner >> 1) & 1u, corner >> 2);
								corners[corner] = glm::dot(Gradient(cell.x + offset.x, cell.y + offset.y, cell.z + offset.z), f - glm::vec3(offset));
							}
							const float y0 = glm::mix(glm::mix(corners[0], corners[1], w.x), glm::mix(corners[2], corners[3], w.x), w.y);
							const float y1 = glm::mix(glm::mix(corners[4], corners[5], w.x), glm::mix(corners[6], corners[7], w.x), w.y);
							values[index++] += amplitude * glm::mix(y0, y1, w.z);
						}
					}
				}
				amplitude *= NOISE_PERSISTENCE;
			}

			// Equalise: replace each value by the fraction of values below it
			constexpr size_t BINS = 4096;
			const auto [lowest, highest] = std::minmax_element(values.begin(), values.end());
			const float low = *lowest, range = std::max(*highest - low, 1e-6f);
			auto Bin = [&](const float value) { return std::min((size_t)((value - low) / range * (float)BINS), BINS - 1); };
			std::vector<size_t> counts(BINS, 0);
			for (const float value : values)
				counts[Bin(value)]++;
			std::vector<unsigned char> levels(BINS);
			size_t below = 0;
			for (size_t bin = 0; bin < BINS; bin++)
			{
				levels[bin] = (unsigned char)std::lround(((float)below + 0.5f * (float)counts[bin]) / (float)VOXELS * 255.0f);
				below += counts[bin];
			}
			for (size_t i = 0; i < VOXELS; i++)
				data[i * 4 + channel] = levels[Bin(values[i])];
		};

		// The channels are independent: make them at the same time
		std::array<std::thread, 4> threads;
		for (uint32_t channel = 0; channel < 4; channel++)
			threads[channel] = std::thread(CreateChannel, channel);
		for (std::thread& thread : threads)
			thread.join();
		return data;
	}
	}

	sRef<DH::Texture3D> GetNoiseVolume()
	{
		// Shared by the renderers alive at the same time (it takes a moment to make)
		static std::weak_ptr<DH::Texture3D> shared;
		sRef<DH::Texture3D> volume = shared.lock();
		if (volume == nullptr)
		{
			const std::vector<unsigned char> data = CreateNoiseData();
			volume = DH::Texture3D::Create({ NOISE_SIZE, NOISE_SIZE, NOISE_SIZE }, data.data(), DH::Format::RGBA8);
			shared = volume;
		}
		return volume;
	}

	sRef<DH::Sampler> CreateNoiseSampler()
	{
		DH::SamplerDesc desc;
		desc.addressU = DH::AddressMode::Repeat;
		desc.addressV = DH::AddressMode::Repeat;
		desc.addressW = DH::AddressMode::Repeat;
		return DH::Sampler::Create(desc);
	}
}
