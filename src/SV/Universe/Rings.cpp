#include "Rings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <glm/gtc/packing.hpp>

namespace SV::Rings
{
	namespace
	{
		// Rows of each map: all the same (only v = 0.5 is read)
		constexpr unsigned int MAP_HEIGHT = 4;
		// Samples per dust map texel, so narrow ringlets are averaged rather than missed
		constexpr int DUST_SUBSAMPLES = 16;

		constexpr double GAMMA = 2.2;

		unsigned char ToByte(double value)
		{
			return (unsigned char)std::lround(std::clamp(value, 0.0, 1.0) * 255.0);
		}

		template<typename T>
		std::vector<unsigned char> RepeatRows(const std::vector<T>& row)
		{
			const size_t rowBytes = row.size() * sizeof(T);
			std::vector<unsigned char> data(rowBytes * MAP_HEIGHT);
			for (unsigned int y = 0; y < MAP_HEIGHT; y++)
				std::memcpy(data.data() + y * rowBytes, row.data(), rowBytes);
			return data;
		}
	}

	double Band(const double radius, const double inner, const double outer, const double edge)
	{
		if (radius >= inner && radius <= outer)
			return 1.0;
		if (edge <= 0.0)
			return 0.0;
		const double distance = radius < inner ? inner - radius : radius - outer;
		const double t = std::clamp(1.0 - distance / edge, 0.0, 1.0);
		return t * t * (3.0 - 2.0 * t);
	}

	double Gaussian(const double radius, const double centre, const double innerSigma, const double outerSigma)
	{
		const double x = (radius - centre) / (radius < centre ? innerSigma : outerSigma);
		return std::exp(-x * x);
	}

	sRef<AsyncTexture> CreateRingMap(const double innerRadius, const double outerRadius,
									 const std::span<const Ringlet> ringlets, const unsigned int width)
	{
		const double texelWidth = (outerRadius - innerRadius) / width;
		std::vector<uint32_t> row(width);
		for (unsigned int x = 0; x < width; x++)
		{
			const double texelInner = innerRadius + x * texelWidth;
			const double texelOuter = texelInner + texelWidth;

			// Opacity and opacity weighted albedo (gamma encoded), each ringlet weighted by the
			// fraction of the texel it covers
			double opacity = 0.0;
			glm::dvec3 colour(0.0);
			for (const Ringlet& ringlet : ringlets)
			{
				const double overlap = std::min(texelOuter, ringlet.centre + ringlet.width * 0.5) -
									   std::max(texelInner, ringlet.centre - ringlet.width * 0.5);
				if (overlap <= 0.0)
					continue;
				const double coverage = overlap / texelWidth * (1.0 - std::exp(-ringlet.opticalDepth));
				opacity += coverage;
				colour += coverage * glm::pow(glm::dvec3(ringlet.albedo), glm::dvec3(1.0 / GAMMA));
			}
			if (opacity > 1.0)
			{
				colour /= opacity;
				opacity = 1.0;
			}
			row[x] = (uint32_t)ToByte(colour.r) | (uint32_t)ToByte(colour.g) << 8 | (uint32_t)ToByte(colour.b) << 16 |
					 (uint32_t)ToByte(opacity) << 24;
		}
		return make<AsyncTexture>(DH::Dimension2D(width, MAP_HEIGHT), RepeatRows(row), DH::Format::RGBA8);
	}

	sRef<AsyncTexture> CreateDustMap(const double outerRadius, const DustProfile& profile, const unsigned int width)
	{
		const double texelWidth = outerRadius / width;
		std::vector<uint64_t> row(width);
		for (unsigned int x = 0; x < width; x++)
		{
			glm::dvec3 depth(0.0);
			for (int s = 0; s < DUST_SUBSAMPLES; s++)
				depth += profile((x + (s + 0.5) / DUST_SUBSAMPLES) * texelWidth);
			depth /= DUST_SUBSAMPLES * DUST_DEPTH_UNIT;
			row[x] = glm::packHalf4x16(glm::vec4(glm::vec3(depth), 0.0f));
		}
		return make<AsyncTexture>(DH::Dimension2D(width, MAP_HEIGHT), RepeatRows(row), DH::Format::RGBA16F);
	}

	void ConvertPhotographicRingMap(DH::TextureLoader& image)
	{
		const DH::Dimension2D& size = image.GetDimensions();
		unsigned char* texels = image.GetData();
		for (size_t i = 0; i < size.x * size.y; i++)
		{
			unsigned char* texel = texels + i * 4;
			const double opacity = texel[3] / 255.0;

			// The map's colour darkens with opacity; undo that for the particles' own colour (scaled
			// to plausible icy albedos). Nearly transparent texels have arbitrary colours, so faint
			// rings take a neutral grey.
			const glm::dvec3 colour = glm::dvec3(texel[0], texel[1], texel[2]) / 255.0;
			const glm::dvec3 mapAlbedo = glm::clamp(glm::pow(colour / std::max(opacity, 0.5), glm::dvec3(GAMMA)) * 2.0,
													0.0, 0.9);
			const double t = std::clamp((opacity - 0.02) / (0.15 - 0.02), 0.0, 1.0);
			const glm::dvec3 albedo = glm::mix(glm::dvec3(0.5), mapAlbedo, t * t * (3.0 - 2.0 * t));

			const glm::dvec3 encoded = opacity * glm::pow(albedo, glm::dvec3(1.0 / GAMMA));
			texel[0] = ToByte(encoded.r);
			texel[1] = ToByte(encoded.g);
			texel[2] = ToByte(encoded.b);
		}
	}
}
