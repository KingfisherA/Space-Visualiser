#pragma once
#include <functional>
#include <span>

#include <glm/glm.hpp>

#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/AsyncTexture.hpp"

// Radial profiles of planetary rings, made into the textures Shaders/Include/Rings.glsl samples.
// A ring system has up to two maps, both one dimensional (the rings are taken to be axisymmetric):
//  - The ring map: rings of macroscopic (cm to m) particles, which are dense enough to cast
//    shadows. RGBA8: a = normal opacity, rgb = the particles' albedo with a 2.2 gamma, multiplied
//    by the opacity (so mip maps average it by opacity).
//  - The dust map: faint rings of micron sized dust, seen mostly against the Sun. RGBA16F: rgb =
//    normal optical depth per colour channel in units of DUST_DEPTH_UNIT, from the planet's centre
//    (u = 0) to the outer edge of the dust (u = 1).
namespace SV::Rings
{
	// Mirrors DUST_DEPTH_UNIT in Shaders/Include/Rings.glsl
	constexpr double DUST_DEPTH_UNIT = 1E-5;

	// A ring of macroscopic particles (radii in km)
	struct Ringlet
	{
		double centre;
		double width;
		double opticalDepth; // Normal optical depth
		glm::vec3 albedo;    // Single scattering albedo of the particles
	};

	// A ring map spanning innerRadius to outerRadius (km). Each texel averages the ringlets that
	// cover it, so rings narrower than a texel keep their total effect.
	sRef<AsyncTexture> CreateRingMap(double innerRadius, double outerRadius, std::span<const Ringlet> ringlets,
									 unsigned int width = 4096);

	// Normal optical depth per colour channel of the dust at a radius (km)
	using DustProfile = std::function<glm::dvec3(double radius)>;

	// A dust map from the planet's centre to outerRadius (km)
	sRef<AsyncTexture> CreateDustMap(double outerRadius, const DustProfile& profile, unsigned int width = 4096);

	// Profile shapes (radii in km)
	// 1 between inner and outer, falling smoothly to 0 over `edge` outside them
	double Band(double radius, double inner, double outer, double edge);
	// Peak of 1 at centre, with a different standard deviation on each side
	double Gaussian(double radius, double centre, double innerSigma, double outerSigma);

	// Turns a photographic ring map (RGBA8: display colour, a = opacity) into the ring map
	// encoding, estimating the particles' albedo from the colour
	void ConvertPhotographicRingMap(DH::TextureLoader& image);
}
