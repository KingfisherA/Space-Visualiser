#include "Nebula.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "SV/Graphics/NoiseVolume.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "Star.hpp"

namespace SV
{
	namespace
	{
		// Mirrors NebulaUniforms in Shaders/Nebula.frag (std140)
		struct NebulaUniforms
		{
			glm::vec4 centre;  // xyz = the nebula's centre (render coordinates), w = time
			glm::vec4 shape;   // xyz = semi-axes, w = kind (NebulaKind)
			glm::vec4 form;    // x = tilt (radians), y = detail, z = turbulence, w = filament sharpness
			glm::vec4 lines;   // x = hydrogen, y = oxygen, z = nitrogen and sulphur, w = continuum
			glm::vec4 look;    // x = dust, y = filaments, z = star field brightness, w = pixel angle
			glm::vec4 bounds;  // x = bounding radius, y = star count, z = steps, w = cloud count
			glm::vec4 light;   // xyz = towards the stars lighting the pillars, w = the length of a step across the bounds
			glm::vec4 stretch; // xyz = how much the structure is drawn out along each axis
			glm::vec4 bubble;  // xyz = centre, w = radius
			glm::vec4 bubbleShell; // x = thickness, y = brightness, z = clearing, w = oxygen
			glm::vec4 hydrogenColour;
			glm::vec4 oxygenColour;
			glm::vec4 sulphurColour;
			std::array<glm::vec4, MAX_NEBULA_CLOUDS> cloudBase; // xyz = base, w = radius there
			std::array<glm::vec4, MAX_NEBULA_CLOUDS> cloudHead; // xyz = head, w = radius there
			std::array<glm::vec4, MAX_NEBULA_CLOUDS> cloudInfo; // x = density
			std::array<glm::vec4, MAX_NEBULA_STARS> stars;    // xyz = position, w = brightness
			std::array<glm::vec4, MAX_NEBULA_STARS> starInfo; // x = temperature, y = ionising radius, z = ionising strength
			std::array<glm::vec4, BLACKBODY_TABLE_SIZE> blackbody;
		};

		// The gas is raymarched within a sphere this much larger than its largest semi-axis: a
		// supernova remnant's ends there, but an H II region's cloud goes on behind it, and a
		// planetary nebula has a faint halo. Its clouds (or lobes) and bubble are within it too,
		// with this much more around them.
		constexpr float REMNANT_BOUNDS = 1.25f;
		constexpr float SHELL_BOUNDS = 1.2f;
		constexpr float PLANETARY_BOUNDS = 1.8f;
		constexpr float HII_REGION_BOUNDS = 1.6f;
		constexpr float CLOUD_BOUNDS = 1.15f;
		// Steps along a ray, at most (mirrors MAX_STEPS in Shaders/Nebula.frag)
		constexpr int MAX_STEPS = 320;
		// The time is kept within this, so it stays precise in a float
		constexpr double TIME_WRAP = 1000.0;

		// The emission lines' colours (linear sRGB, luminance about 1), as they are: H-alpha with
		// some H-beta (pink-red), [O III] (green-blue), and [N II] and [S II] (deep red to orange)
		constexpr glm::vec3 HYDROGEN_COLOUR{ 2.4f, 0.35f, 0.55f };
		constexpr glm::vec3 OXYGEN_COLOUR{ 0.15f, 1.15f, 0.95f };
		constexpr glm::vec3 SULPHUR_COLOUR{ 2.6f, 0.5f, 0.12f };
		// And in the Hubble palette: [S II] red, hydrogen green, [O III] blue
		constexpr glm::vec3 HUBBLE_HYDROGEN_COLOUR{ 0.3f, 1.15f, 0.25f };
		constexpr glm::vec3 HUBBLE_OXYGEN_COLOUR{ 0.1f, 0.55f, 2.4f };
		constexpr glm::vec3 HUBBLE_SULPHUR_COLOUR{ 2.8f, 0.3f, 0.1f };

		// The radius of the sphere the gas is raymarched within
		float BoundingRadius(const NebulaParameters& p)
		{
			const float factor = p.kind == NebulaKind::SupernovaRemnant ? REMNANT_BOUNDS
								 : p.kind == NebulaKind::SupernovaShell	 ? SHELL_BOUNDS
								 : p.kind == NebulaKind::PlanetaryNebula ? PLANETARY_BOUNDS
																		 : HII_REGION_BOUNDS;
			float radius = factor * std::max({ p.size.x, p.size.y, p.size.z });
			for (size_t i = 0; i < std::min(p.clouds.size(), MAX_NEBULA_CLOUDS); i++)
			{
				const NebulaCloud& cloud = p.clouds[i];
				const float reach = std::max(glm::length(cloud.base) + cloud.baseRadius, glm::length(cloud.head) + cloud.headRadius);
				radius = std::max(radius, CLOUD_BOUNDS * reach);
			}
			if (p.bubble.radius > 0.0f)
				radius = std::max(radius, CLOUD_BOUNDS * (glm::length(p.bubble.centre) + p.bubble.radius + 2.0f * p.bubble.thickness));
			return radius;
		}

		// The uniforms for drawing a nebula with these parameters, centred at `centre` (render
		// coordinates), at a time (s), with pixels this many radians across
		NebulaUniforms MakeUniforms(const NebulaParameters& p, const glm::vec3& centre, const float time, const float pixelAngle)
		{
			const size_t starCount = std::min(p.stars.size(), MAX_NEBULA_STARS);
			const size_t cloudCount = std::min(p.clouds.size(), MAX_NEBULA_CLOUDS);
			const float bounds = BoundingRadius(p);
			const int steps = std::clamp(p.steps, 16, MAX_STEPS);
			const NebulaBubble& bubble = p.bubble;
			NebulaUniforms uniforms{
				glm::vec4(centre, time),
				glm::vec4(p.size, (float)p.kind),
				glm::vec4(glm::radians(p.tilt), p.detail, p.turbulence, p.filamentSharpness),
				glm::vec4(p.hydrogen, p.oxygen, p.sulphur, p.continuum),
				glm::vec4(p.dust, p.filaments, p.starFieldBrightness, pixelAngle),
				glm::vec4(bounds, (float)starCount, (float)steps, (float)cloudCount),
				glm::vec4(glm::normalize(p.lightDirection), 2.0f * bounds / (float)steps),
				glm::vec4(p.stretch, 0.0f),
				glm::vec4(bubble.centre, bubble.radius),
				glm::vec4(std::max(bubble.thickness, 1e-3f), bubble.brightness, bubble.clearing, bubble.oxygen),
				glm::vec4(p.hubblePalette ? HUBBLE_HYDROGEN_COLOUR : HYDROGEN_COLOUR, 0.0f),
				glm::vec4(p.hubblePalette ? HUBBLE_OXYGEN_COLOUR : OXYGEN_COLOUR, 0.0f),
				glm::vec4(p.hubblePalette ? HUBBLE_SULPHUR_COLOUR : SULPHUR_COLOUR, 0.0f),
				{},
				{},
				{},
				{},
				{},
				BlackbodyTable()
			};
			for (size_t i = 0; i < cloudCount; i++)
			{
				const NebulaCloud& cloud = p.clouds[i];
				uniforms.cloudBase[i] = glm::vec4(cloud.base, cloud.baseRadius);
				uniforms.cloudHead[i] = glm::vec4(cloud.head, cloud.headRadius);
				uniforms.cloudInfo[i] = glm::vec4(cloud.density, 0.0f, 0.0f, 0.0f);
			}
			for (size_t i = 0; i < starCount; i++)
			{
				const NebulaStar& star = p.stars[i];
				uniforms.stars[i] = glm::vec4(star.position, star.brightness);
				uniforms.starInfo[i] = glm::vec4(star.temperature, star.ionisingRadius, star.ionisingStrength, 0.0f);
			}
			return uniforms;
		}

		NebulaPreset Crab()
		{
			// M1, 2 kpc away: 4.4 by 2.9 pc, its long axis from north-west to south-east
			NebulaPreset preset;
			preset.name = "Crab Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::SupernovaRemnant;
			p.size = { 2.2f, 1.45f, 1.5f };
			p.tilt = 35.0f;
			p.hydrogen = 0.8f;
			p.oxygen = 0.7f;
			p.sulphur = 1.2f;
			p.continuum = 1.0f;
			p.dust = 0.4f;
			p.detail = 0.3f;
			p.filaments = 1.0f;
			p.filamentSharpness = 10.0f;
			p.turbulence = 0.5f;
			p.stars = {
				{ "Crab Pulsar", { 0.0f, 0.0f, 0.0f }, 0.8f, 25000.0f },
				{ "", { 0.04f, 0.06f, 0.4f }, 0.6f, 6500.0f }, // The star beside the pulsar (in the line of sight)
				{ "", { 1.3f, -1.6f, 3.0f }, 0.5f, 5200.0f },
				{ "", { -2.1f, 0.9f, 2.0f }, 0.35f, 8000.0f },
				{ "", { 2.7f, 1.4f, -2.0f }, 0.3f, 4200.0f },
			};
			preset.overviewZoom = 9.0f;
			return preset;
		}

		NebulaPreset Orion()
		{
			// M42, 414 pc away: about 7 pc across, around the Trapezium (with M43 to the north)
			NebulaPreset preset;
			preset.name = "Orion Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::HIIRegion;
			p.size = { 4.0f, 3.6f, 2.2f };
			p.tilt = 0.0f;
			p.hydrogen = 1.0f;
			p.oxygen = 1.0f;
			p.sulphur = 0.6f;
			p.continuum = 0.25f;
			p.dust = 1.0f;
			p.brightness = 1.6f;
			p.detail = 0.3f;
			p.turbulence = 0.6f;
			p.stars = {
				// The Trapezium: Theta-1 Orionis C (which ionises most of the nebula), A, B and D
				{ "Trapezium", { 0.0f, 0.0f, 0.05f }, 4.0f, 39000.0f, 1.2f, 1.0f },
				{ "", { 0.03f, 0.035f, 0.08f }, 1.6f, 30000.0f },
				{ "", { -0.035f, 0.025f, 0.0f }, 1.2f, 25000.0f },
				{ "", { 0.02f, -0.04f, 0.04f }, 1.6f, 32000.0f },
				{ "Theta-2 Orionis", { 0.42f, -0.5f, 0.25f }, 2.2f, 34000.0f, 0.4f, 0.15f },
				// NU Orionis, which ionises M43
				{ "M43", { 0.35f, 2.0f, -0.25f }, 2.0f, 30000.0f, 0.55f, 0.35f },
				// Members of the Orion Nebula Cluster
				{ "", { -0.3f, 0.25f, 0.1f }, 0.5f, 5000.0f },
				{ "", { 0.45f, 0.3f, -0.05f }, 0.4f, 4200.0f },
				{ "", { -0.15f, -0.4f, 0.2f }, 0.6f, 9000.0f },
				{ "", { 0.25f, -0.15f, 0.3f }, 0.35f, 3800.0f },
				{ "", { -0.55f, -0.1f, 0.0f }, 0.45f, 6500.0f },
				{ "", { 0.1f, 0.5f, 0.15f }, 0.3f, 4500.0f },
				{ "", { -0.4f, 0.6f, -0.1f }, 0.35f, 7000.0f },
				{ "", { 0.7f, 0.05f, 0.1f }, 0.3f, 5500.0f },
			};
			preset.overviewZoom = 11.0f;
			return preset;
		}

		NebulaPreset PillarsOfCreation()
		{
			// In M16 (the Eagle Nebula), 1.7 kpc away: three pillars, the tallest 1.3 pc, worn away
			// from above by the hot stars of NGC 6611. Shown in the Hubble palette, as in Hubble's
			// image of them.
			NebulaPreset preset;
			preset.name = "Pillars of Creation";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::Pillars;
			p.size = { 1.5f, 1.3f, 1.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 1.0f;
			p.sulphur = 1.0f;
			p.continuum = 0.0f;
			p.dust = 1.0f;
			p.detail = 0.8f;
			p.turbulence = 0.12f;
			p.clouds = {
				{ { -0.6f, -1.15f, 0.0f }, { -0.38f, 0.42f, 0.05f }, 0.27f, 0.15f },
				{ { 0.08f, -1.15f, -0.1f }, { 0.14f, -0.12f, -0.05f }, 0.2f, 0.13f },
				{ { 0.62f, -1.15f, -0.2f }, { 0.56f, -0.46f, -0.12f }, 0.16f, 0.1f },
			};
			p.lightDirection = glm::normalize(glm::vec3(0.1f, 1.0f, 0.35f));
			p.hubblePalette = true;
			p.stars = {
				{ "", { 0.9f, 0.7f, 0.6f }, 0.6f, 9000.0f },
				{ "", { -1.0f, -0.3f, 0.5f }, 0.4f, 5000.0f },
				{ "", { 0.3f, 0.9f, 0.3f }, 0.35f, 15000.0f },
				{ "", { -0.2f, -0.8f, 0.7f }, 0.3f, 4200.0f },
				{ "", { 1.2f, -0.6f, 0.2f }, 0.45f, 7000.0f },
			};
			p.brightness = 1.4f;
			preset.overviewZoom = 3.4f;
			return preset;
		}
		NebulaPreset Bubble()
		{
			// NGC 7635 in Cassiopeia, about 2.7 kpc away: a bubble 2 pc across blown by the wind of
			// BD+60 2522 (a young O star, off the bubble's centre) in the gas of the H II region
			// around it, with dense clumps of the molecular cloud to the west lit by it
			NebulaPreset preset;
			preset.name = "Bubble Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 3.4f, 2.6f, 2.4f };
			p.hydrogen = 1.0f;
			p.oxygen = 1.0f;
			p.sulphur = 0.6f;
			p.continuum = 0.15f;
			p.dust = 1.0f;
			p.detail = 1.1f;
			p.turbulence = 0.25f;
			p.filaments = 0.25f;
			p.filamentSharpness = 4.0f;
			p.bubble = { { 0.0f, 0.0f, 0.0f }, 1.05f, 0.07f, 3.0f, 0.9f, 0.35f };
			p.clouds = {
				// Clumps of the molecular cloud to the west, their faces towards the star lit
				{ { 2.1f, 1.1f, 0.2f }, { 1.45f, 0.7f, 0.5f }, 0.55f, 0.3f, 1.0f },
				{ { 2.3f, -1.0f, 0.3f }, { 1.6f, -0.75f, 0.5f }, 0.45f, 0.25f, 1.0f },
				{ { 1.2f, 1.05f, 0.6f }, { 1.2f, 1.05f, 0.6f }, 0.15f, 0.15f, 1.0f },
				{ { 1.5f, -0.1f, 0.9f }, { 1.5f, -0.1f, 0.9f }, 0.12f, 0.12f, 1.0f },
			};
			p.stars = {
				{ "BD+60 2522", { -0.3f, 0.28f, 0.1f }, 3.0f, 37000.0f, 1.4f, 1.0f },
				{ "", { 1.6f, -1.4f, 0.8f }, 0.4f, 5000.0f },
				{ "", { -2.2f, 1.1f, -0.5f }, 0.35f, 8000.0f },
				{ "", { 0.9f, 1.9f, 0.4f }, 0.3f, 4200.0f },
				{ "", { -1.0f, -1.2f, 1.2f }, 0.3f, 6500.0f },
			};
			p.brightness = 1.5f;
			preset.overviewZoom = 7.5f;
			return preset;
		}

		NebulaPreset BlueFlash()
		{
			// NGC 6905 in Delphinus, about 2.6 kpc away: a planetary nebula whose bright, oxygen
			// blue shell (0.4 pc long, its long axis from north-west to south-east) has fainter lobes
			// blown out at its ends, tipped with knots, around one of the hottest of central stars
			NebulaPreset preset;
			preset.name = "Blue Flash Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::PlanetaryNebula;
			p.size = { 0.14f, 0.2f, 0.14f };
			p.tilt = -35.0f;
			p.hydrogen = 0.8f;
			p.oxygen = 1.6f;
			p.sulphur = 0.8f;
			p.continuum = 0.0f;
			p.dust = 0.5f;
			p.detail = 5.0f;
			p.turbulence = 0.03f;
			p.clouds = {
				{ { 0.0f, 0.1f, 0.0f }, { 0.02f, 0.36f, 0.03f }, 0.11f, 0.05f, 0.6f },
				{ { 0.0f, -0.1f, 0.0f }, { -0.02f, -0.36f, -0.03f }, 0.11f, 0.05f, 0.6f },
			};
			p.stars = {
				{ "HD 193949", { 0.0f, 0.0f, 0.0f }, 1.2f, 40000.0f },
				{ "", { 0.35f, -0.3f, 0.4f }, 0.3f, 5200.0f },
				{ "", { -0.4f, 0.25f, -0.3f }, 0.25f, 7000.0f },
			};
			p.brightness = 1.4f;
			preset.overviewZoom = 1.2f;
			return preset;
		}

		NebulaPreset Box()
		{
			// NGC 6309 in Ophiuchus, about 2 kpc away: a quadrupolar planetary nebula, two pairs of
			// lobes blown out along axes at an angle to each other (so its bright core is box shaped),
			// each tipped with a knot
			NebulaPreset preset;
			preset.name = "Box Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::PlanetaryNebula;
			p.size = { 0.06f, 0.11f, 0.06f };
			p.tilt = -20.0f;
			p.hydrogen = 1.0f;
			p.oxygen = 1.3f;
			p.sulphur = 0.9f;
			p.continuum = 0.0f;
			p.dust = 0.5f;
			p.detail = 7.0f;
			p.turbulence = 0.02f;
			p.clouds = {
				// The box: two pairs of lobes at an angle to each other
				{ { 0.0f, 0.0f, 0.0f }, { 0.065f, 0.14f, 0.01f }, 0.05f, 0.035f, 0.9f },
				{ { 0.0f, 0.0f, 0.0f }, { -0.065f, -0.14f, -0.01f }, 0.05f, 0.035f, 0.9f },
				{ { 0.0f, 0.0f, 0.0f }, { -0.06f, 0.13f, -0.01f }, 0.045f, 0.03f, 0.7f },
				{ { 0.0f, 0.0f, 0.0f }, { 0.06f, -0.13f, 0.01f }, 0.045f, 0.03f, 0.7f },
				// Fainter, point symmetric arms beyond its corners
				{ { 0.06f, 0.13f, 0.01f }, { 0.02f, 0.27f, 0.03f }, 0.03f, 0.022f, 0.35f },
				{ { -0.06f, -0.13f, -0.01f }, { -0.02f, -0.27f, -0.03f }, 0.03f, 0.022f, 0.35f },
			};
			p.stars = {
				{ "HD 155752", { 0.0f, 0.0f, 0.0f }, 0.8f, 40000.0f },
				{ "", { 0.3f, 0.25f, -0.3f }, 0.25f, 4800.0f },
				{ "", { -0.25f, -0.3f, 0.35f }, 0.2f, 6500.0f },
			};
			p.brightness = 1.4f;
			preset.overviewZoom = 0.8f;
			return preset;
		}

		NebulaPreset Veil()
		{
			// The Cygnus Loop, 735 pc away: the shell of a supernova of 10 to 20 thousand years ago,
			// 40 pc across, brightest in its arcs on the east (the Eastern Veil) and west (the Western
			// Veil, by 52 Cygni, a foreground star) and in Pickering's Triangle in the north
			NebulaPreset preset;
			preset.name = "Veil Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::SupernovaShell;
			p.size = { 22.0f, 18.0f, 20.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 1.8f;
			p.sulphur = 0.6f;
			p.continuum = 0.0f;
			p.dust = 0.3f;
			p.detail = 0.12f;
			p.turbulence = 1.0f;
			p.filaments = 1.0f;
			p.filamentSharpness = 90.0f;
			p.stars = {
				{ "52 Cygni", { 15.0f, -3.0f, 22.0f }, 2.5f, 4700.0f },
				{ "", { -8.0f, 12.0f, 10.0f }, 0.4f, 6000.0f },
				{ "", { 5.0f, 9.0f, -14.0f }, 0.35f, 9000.0f },
				{ "", { -14.0f, -6.0f, 6.0f }, 0.3f, 4300.0f },
				{ "", { 3.0f, -12.0f, 12.0f }, 0.35f, 5500.0f },
			};
			p.brightness = 2.2f;
			p.steps = 300;
			preset.overviewZoom = 65.0f;
			return preset;
		}

		NebulaPreset NorthAmerica()
		{
			// NGC 7000 in Cygnus, 800 pc away, with the Pelican Nebula (IC 5070) to its west: one H II
			// region, 30 pc across, ionised by the Bajamar star (an O star hidden by the dust) and cut
			// in two by the dark cloud LDN 935 in front of it, whose edge (the Cygnus Wall) is the
			// "Gulf of Mexico" and "Central America"
			NebulaPreset preset;
			preset.name = "North America Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 16.0f, 13.0f, 4.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 0.5f;
			p.sulphur = 0.7f;
			p.continuum = 0.1f;
			p.dust = 1.0f;
			p.detail = 0.2f;
			p.turbulence = 1.5f;
			p.filaments = 0.3f;
			p.filamentSharpness = 4.0f;
			p.clouds = {
				// LDN 935: the "Atlantic", between the North America and Pelican nebulae
				{ { 5.5f, 13.0f, 3.0f }, { 5.5f, -3.0f, 3.0f }, 2.8f, 2.5f, 1.0f },
				{ { 5.5f, -3.0f, 3.0f }, { 8.0f, -14.0f, 2.5f }, 2.5f, 3.0f, 1.0f },
				// The "Gulf of Mexico", with "Florida" between it and the Atlantic
				{ { -4.0f, -3.4f, 2.5f }, { 0.2f, -4.8f, 2.5f }, 2.3f, 1.9f, 1.0f },
				{ { 0.2f, -5.6f, 2.5f }, { 4.5f, -8.0f, 2.8f }, 1.5f, 1.8f, 1.0f },
				// Beyond "Central America"
				{ { -9.0f, -13.5f, 2.0f }, { -1.0f, -15.0f, 2.0f }, 2.5f, 2.5f, 0.8f },
				// The Pelican's head and neck: a cloud whose face towards the star is lit
				{ { 11.5f, 4.0f, 0.5f }, { 12.0f, -3.5f, 0.5f }, 1.8f, 1.6f, 1.0f },
			};
			p.stars = {
				{ "Bajamar star", { 1.5f, -4.2f, -0.5f }, 0.8f, 40000.0f, 14.0f, 1.2f },
				{ "", { -6.0f, 4.0f, 3.0f }, 0.5f, 5000.0f },
				{ "", { 8.0f, -6.0f, 4.0f }, 0.4f, 9000.0f },
				{ "", { -9.0f, -5.0f, -2.0f }, 0.35f, 4200.0f },
				{ "", { 2.0f, 7.0f, 4.0f }, 0.4f, 6500.0f },
				{ "", { 13.0f, 6.0f, 1.0f }, 0.3f, 7500.0f },
			};
			p.brightness = 2.0f;
			preset.overviewZoom = 45.0f;
			return preset;
		}

		NebulaPreset Carina()
		{
			// NGC 3372, 2.35 kpc away: one of the largest H II regions in the galaxy, lit by the
			// clusters Trumpler 14 and 16 and Eta Carinae, cut by a V of dust in front of it, with the
			// Keyhole (a dark cloud by Eta Carinae) and the South Pillars facing the clusters
			NebulaPreset preset;
			preset.name = "Carina Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 22.0f, 18.0f, 7.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 1.1f;
			p.sulphur = 0.6f;
			p.continuum = 0.1f;
			p.dust = 1.0f;
			p.detail = 0.1f;
			p.turbulence = 3.0f;
			p.filaments = 0.35f;
			p.filamentSharpness = 5.0f;
			p.clouds = {
				// The Keyhole
				{ { 0.9f, 0.5f, 1.5f }, { 1.8f, 1.4f, 1.5f }, 0.7f, 0.6f, 1.2f },
				{ { -0.2f, 1.0f, 1.2f }, { -0.2f, 1.0f, 1.2f }, 0.45f, 0.45f, 1.2f },
				// The V of dust in front, opening to the east
				{ { -2.5f, -2.5f, 5.0f }, { -16.0f, 5.0f, 4.5f }, 1.4f, 2.4f, 1.0f },
				{ { -2.5f, -2.5f, 5.0f }, { -14.0f, -11.0f, 4.5f }, 1.4f, 2.4f, 1.0f },
				// The South Pillars, pointing at the clusters
				{ { 2.5f, -14.0f, 2.5f }, { 1.0f, -6.0f, 2.5f }, 2.0f, 0.8f, 1.0f },
				{ { 7.5f, -12.5f, 2.0f }, { 5.0f, -6.5f, 2.0f }, 1.6f, 0.65f, 1.0f },
				{ { -4.0f, -14.5f, 2.5f }, { -2.5f, -8.0f, 2.5f }, 1.7f, 0.7f, 1.0f },
			};
			p.stars = {
				{ "Eta Carinae", { 0.0f, 0.0f, 0.5f }, 8.0f, 9000.0f, 6.0f, 1.0f },
				// Trumpler 16, around Eta Carinae
				{ "Trumpler 16", { -1.5f, -1.0f, 0.0f }, 2.0f, 40000.0f, 9.0f, 0.6f },
				{ "", { 1.2f, -1.8f, -0.5f }, 1.5f, 36000.0f, 9.0f, 0.4f },
				{ "", { -0.8f, 1.6f, 0.4f }, 1.2f, 33000.0f },
				// Trumpler 14
				{ "Trumpler 14", { 6.0f, 6.5f, 0.0f }, 2.5f, 40000.0f, 7.0f, 1.0f },
				{ "", { 6.6f, 6.0f, 0.5f }, 1.5f, 38000.0f, 4.0f, 0.4f },
				{ "", { -9.0f, 8.0f, 4.0f }, 0.5f, 5000.0f },
				{ "", { 12.0f, -4.0f, 3.0f }, 0.45f, 8000.0f },
				{ "", { -6.0f, -12.0f, 5.0f }, 0.4f, 4300.0f },
				{ "", { 9.0f, 11.0f, -3.0f }, 0.35f, 6500.0f },
				{ "", { -13.0f, -2.0f, -4.0f }, 0.4f, 12000.0f },
			};
			p.brightness = 1.8f;
			preset.overviewZoom = 60.0f;
			return preset;
		}

		NebulaPreset Rim()
		{
			// NGC 6188 in Ara, about 1.2 kpc away: the edge of a dark molecular cloud, its face lit
			// and ionised by the hot young stars of NGC 6193 to the east, worn into ridges and
			// "dragons" of dust standing against the glowing gas
			NebulaPreset preset;
			preset.name = "Rim Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 5.0f, 3.6f, 3.0f };
			p.hydrogen = 1.0f;
			p.oxygen = 0.4f;
			p.sulphur = 0.9f;
			p.continuum = 0.1f;
			p.dust = 1.0f;
			p.detail = 0.35f;
			p.turbulence = 0.6f;
			p.filaments = 0.4f;
			p.filamentSharpness = 5.0f;
			p.clouds = {
				{ { 2.0f, -3.6f, 0.0f }, { -3.2f, -2.6f, -0.5f }, 2.4f, 2.0f, 1.0f },
				{ { 0.5f, -1.7f, 0.2f }, { -0.6f, -0.2f, 0.4f }, 0.7f, 0.35f, 1.0f },
				{ { 2.7f, -2.1f, 0.0f }, { 2.0f, -0.5f, 0.2f }, 0.8f, 0.4f, 1.0f },
				{ { -2.4f, -1.6f, 0.0f }, { -3.0f, -0.4f, 0.1f }, 0.6f, 0.3f, 1.0f },
				{ { -0.5f, 1.6f, 1.2f }, { 2.5f, 0.8f, 1.0f }, 0.4f, 0.35f, 0.5f },
			};
			p.stars = {
				{ "NGC 6193", { -3.6f, 2.0f, 0.5f }, 2.5f, 40000.0f, 4.0f, 1.0f },
				{ "", { -3.2f, 2.4f, 0.3f }, 1.8f, 38000.0f, 3.0f, 0.5f },
				{ "", { 2.5f, 1.8f, 1.5f }, 0.4f, 5200.0f },
				{ "", { -1.5f, -2.5f, 2.0f }, 0.3f, 7000.0f },
				{ "", { 3.6f, -0.4f, -1.0f }, 0.35f, 4300.0f },
			};
			p.brightness = 1.6f;
			preset.overviewZoom = 10.0f;
			return preset;
		}

		NebulaPreset Rosette()
		{
			// NGC 2237 to 2246 in Monoceros, 1.5 kpc away: a shell of gas 35 pc across around a cavity
			// blown by the winds of the young cluster NGC 2244 at its centre, with dark globules and
			// trunks of dust on its inner edge, most in the north-west
			NebulaPreset preset;
			preset.name = "Rosette Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 15.0f, 14.0f, 3.5f };
			p.hydrogen = 1.0f;
			p.oxygen = 0.45f;
			p.sulphur = 0.6f;
			p.continuum = 0.1f;
			p.dust = 1.0f;
			p.detail = 0.15f;
			p.turbulence = 3.0f;
			p.filaments = 0.45f;
			p.filamentSharpness = 4.0f;
			p.bubble = { { 0.0f, 0.0f, 0.0f }, 7.0f, 1.5f, 1.0f, 1.0f, 0.4f };
			p.clouds = {
				{ { 9.5f, 6.0f, 0.0f }, { 5.8f, 3.8f, 0.5f }, 1.2f, 0.4f, 1.0f },
				{ { 7.0f, 9.5f, -1.0f }, { 4.5f, 5.6f, 0.0f }, 1.0f, 0.35f, 1.0f },
				{ { 6.5f, 2.0f, 1.0f }, { 6.5f, 2.0f, 1.0f }, 0.5f, 0.5f, 1.0f },
				{ { 3.5f, 6.8f, 1.5f }, { 3.5f, 6.8f, 1.5f }, 0.4f, 0.4f, 1.0f },
				{ { -9.0f, -8.0f, 2.0f }, { -4.0f, -11.0f, 1.0f }, 1.5f, 1.5f, 0.6f },
			};
			p.stars = {
				{ "NGC 2244", { -1.0f, 0.5f, 0.0f }, 2.0f, 40000.0f, 10.0f, 0.6f },
				{ "", { 0.5f, -0.5f, 1.0f }, 1.6f, 38000.0f, 10.0f, 0.5f },
				{ "", { 1.2f, 1.0f, -0.5f }, 1.4f, 35000.0f, 8.0f, 0.4f },
				{ "", { -0.3f, -1.4f, 0.4f }, 1.2f, 33000.0f, 8.0f, 0.3f },
				{ "12 Monocerotis", { 0.8f, -1.8f, 6.0f }, 2.0f, 4500.0f },
				{ "", { -7.0f, 9.0f, 3.0f }, 0.4f, 5000.0f },
				{ "", { 11.0f, -6.0f, -2.0f }, 0.35f, 8000.0f },
				{ "", { -12.0f, 2.0f, 4.0f }, 0.3f, 4200.0f },
			};
			p.brightness = 2.0f;
			preset.overviewZoom = 50.0f;
			return preset;
		}

		NebulaPreset FlamingStar()
		{
			// IC 405 in Auriga, about 450 pc away: gas and dust lit by AE Aurigae, a hot star thrown
			// out of the Orion Nebula's cluster, passing through it. Its hydrogen glows red in wisps
			// streaming north and south (the flames), and the dust near the star scatters its light blue.
			NebulaPreset preset;
			preset.name = "Flaming Star Nebula";
			NebulaParameters& p = preset.parameters;
			p.kind = NebulaKind::EmissionNebula;
			p.size = { 3.0f, 2.4f, 1.1f };
			p.hydrogen = 1.0f;
			p.oxygen = 0.2f;
			p.sulphur = 0.5f;
			p.continuum = 2.0f;
			p.dust = 1.0f;
			p.detail = 0.9f;
			p.turbulence = 0.4f;
			p.filaments = 0.9f;
			p.filamentSharpness = 7.0f;
			p.stretch = { 1.0f, 4.0f, 1.0f };
			p.clouds = {
				{ { -1.0f, 0.8f, 0.6f }, { -0.3f, -1.3f, 0.7f }, 0.22f, 0.22f, 0.35f },
				{ { 1.3f, -0.6f, 0.8f }, { 1.3f, -0.6f, 0.8f }, 0.3f, 0.3f, 0.4f },
				{ { 0.5f, 0.5f, -0.3f }, { 1.4f, 1.2f, -0.4f }, 0.35f, 0.25f, 0.6f },
			};
			p.stars = {
				{ "AE Aurigae", { 0.3f, 0.2f, 0.3f }, 3.0f, 33000.0f, 1.5f, 1.0f },
				{ "", { -1.4f, -0.9f, 0.8f }, 0.4f, 5000.0f },
				{ "", { 1.8f, 1.2f, -0.6f }, 0.35f, 9000.0f },
				{ "", { -0.6f, 1.6f, 0.9f }, 0.3f, 4300.0f },
				{ "", { 2.1f, -1.4f, 0.4f }, 0.3f, 6500.0f },
			};
			p.brightness = 2.8f;
			preset.overviewZoom = 8.0f;
			return preset;
		}
	}

	const NebulaPreset& GetNebulaPreset(const NebulaType type)
	{
		static const std::array<NebulaPreset, (size_t)NebulaType::Count> presets = {
			Crab(), Orion(), PillarsOfCreation(), Bubble(), BlueFlash(), Box(), Veil(), NorthAmerica(), Carina(), Rim(), Rosette(), FlamingStar()
		};
		return presets[std::min((size_t)type, presets.size() - 1)];
	}

	NebulaRenderer::NebulaRenderer(const SceneContext& contextIn, const NebulaParameters& parametersIn) :
		parameters(parametersIn),
		context(contextIn),
		compositor(contextIn)
	{
		const NebulaUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(NebulaUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);
		noiseTexture = GetNoiseVolume();
		noiseSampler = CreateNoiseSampler();

		// Set 1 of the march: the nebula's parameters and the noise
		nebulaLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler3D, DH::ShaderStage::Fragment } });
		nebulaGroup = DH::BindGroup::Create(nebulaLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, noiseTexture, noiseSampler) });

		DH::GraphicsPipelineDesc marchDesc;
		marchDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle with each pixel's NDC
		marchDesc.fragmentShader = Shaders::Create("Nebula.frag");
		marchDesc.depth = { false, false, DH::CompareOp::Always };
		marchDesc.blend = DH::BlendPreset::None;
		marchDesc.targetLayout = BloomCompositor::GetSceneLayout();
		marchDesc.bindGroupLayouts = { context.frameLayout, nebulaLayout };
		marchPipeline = DH::GraphicsPipeline::Create(marchDesc);
	}

	NebulaRenderer::~NebulaRenderer() = default;

	void NebulaRenderer::Update(const double deltaTime)
	{
		time = std::fmod(time + deltaTime, TIME_WRAP);
	}

	void NebulaRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const NebulaView& view)
	{
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), parameters.renderScale);

		const float pixelAngle = view.pixelAngle * (float)target.GetHeight() / (float)scene.GetHeight();
		const NebulaUniforms uniforms = MakeUniforms(parameters, view.centre, (float)time, pixelAngle);
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*marchPipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *nebulaGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		compositor.Composite(cmd, target, parameters.brightness, parameters.bloom, compositeMode, opacity);
	}
}
