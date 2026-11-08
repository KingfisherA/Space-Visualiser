#include "Star.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Maths/Geometry.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "SolarSystem.hpp"

namespace SV
{
	namespace
	{
		// Mirrors Shaders/Include/StarPushConstants.glsl
		struct StarPushConstants
		{
			glm::mat4 transform;  // Model to world
			glm::vec4 properties; // x = detail octaves (0 for a plain disc), y = flare activity
			glm::vec4 colour;     // rgb = photosphere colour
			glm::vec4 surface;    // x = temperature, y = granules per radius, z = spot contrast, w = view (StarSurfaceView)
		};

		// Mirrors Shaders/Include/CoronaPushConstants.glsl
		struct CoronaPushConstants
		{
			glm::mat4 transform; // The star's model to world transform
			glm::vec4 colour;    // rgb = photosphere (or stylised) colour, w = hydrogen emission intensity
			glm::vec4 activity;  // x = flare activity, y = 1 for the stylised star
		};

		// Mirrors Shaders/Include/FlarePushConstants.glsl
		struct FlarePushConstants
		{
			glm::vec4 star;   // xy = centre (view units), z = disc radius (view units), w = aspect ratio
			glm::vec4 params; // x = strength, y = size scale, z = a pixel (view units), w = ghost strength
			glm::vec4 colour; // rgb = photosphere colour
		};

		static_assert(sizeof(StarPushConstants) <= 128 && sizeof(CoronaPushConstants) <= 128 &&
					  sizeof(FlarePushConstants) <= 128);

		constexpr unsigned int SPHERE_SUBDIVISIONS = 20;
		constexpr float STAR_NOISE_OCTAVES = 3.0f;
		// Granules per stellar radius of the catalogue's stars close up (the Sun's)
		constexpr float CLOSE_UP_GRANULATION = 460.0f;

		// The flare fades in as the star's disc shrinks on screen from FLARE_START_RADIUS to
		// FLARE_FULL_RADIUS pixels (the corona is barely visible by then). From Earth the Sun's disc
		// is a few pixels across.
		constexpr float FLARE_START_RADIUS = 30.0f;
		constexpr float FLARE_FULL_RADIUS = 6.0f;
		// The glare's size follows the star's brightness, as the planets' sunlight does
		// (DisplaySolarIrradiance: distance^-0.7), to this power, within these limits
		constexpr float FLARE_SIZE_POWER = 0.5f;
		constexpr float FLARE_MIN_SIZE = 0.35f, FLARE_MAX_SIZE = 1.6f;
		// The ghosts fade out as the star leaves the view (in NDC), and the glare further out
		constexpr float GHOST_FADE_START = 0.8f, GHOST_FADE_END = 1.1f;
		constexpr float GLARE_FADE_START = 1.2f, GLARE_FADE_END = 2.0f;

		float SmoothStep(const float edge0, const float edge1, const float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		// Area of the intersection of two discs with radii r1 and r2, centres d apart
		float DiscOverlap(const float r1, const float r2, const float d)
		{
			constexpr float PI = 3.14159265358979f;
			if (d >= r1 + r2)
				return 0.0f;
			if (d <= std::abs(r1 - r2))
				return PI * std::min(r1, r2) * std::min(r1, r2);
			const float a1 = std::acos(std::clamp((d * d + r1 * r1 - r2 * r2) / (2.0f * d * r1), -1.0f, 1.0f));
			const float a2 = std::acos(std::clamp((d * d + r2 * r2 - r1 * r1) / (2.0f * d * r2), -1.0f, 1.0f));
			const float kite = std::sqrt(std::max((-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2), 0.0f));
			return r1 * r1 * a1 + r2 * r2 * a2 - 0.5f * kite;
		}

		// Planck's law, for a wavelength (nm) and temperature (K), up to a constant factor
		double Planck(const double wavelength, const double temperature)
		{
			constexpr double C2 = 1.4388e7; // Second radiation constant, nm K
			return 1.0 / (std::pow(wavelength, 5.0) * (std::exp(C2 / (wavelength * temperature)) - 1.0));
		}

		// CIE 1931 colour matching functions (the multi-lobe fit of Wyman, Sloan and Shirley 2013)
		glm::dvec3 ColourMatching(const double wavelength)
		{
			auto lobe = [wavelength](double mean, double below, double above) {
				const double t = (wavelength - mean) / (wavelength < mean ? below : above);
				return std::exp(-0.5 * t * t);
			};
			return {
				1.056 * lobe(599.8, 37.9, 31.0) + 0.362 * lobe(442.0, 16.0, 26.7) - 0.065 * lobe(501.1, 20.4, 26.2),
				0.821 * lobe(568.8, 46.9, 40.5) + 0.286 * lobe(530.9, 16.3, 31.1),
				1.217 * lobe(437.0, 11.8, 36.0) + 0.681 * lobe(459.0, 26.0, 13.8)
			};
		}

		// Photopic luminance of a blackbody's surface (Planck's law integrated against the CIE y
		// function), up to a constant factor
		double SurfaceLuminance(const double temperature)
		{
			double y = 0.0;
			for (double wavelength = 380.0; wavelength <= 780.0; wavelength += 5.0)
				y += ColourMatching(wavelength).y * Planck(wavelength, std::max(temperature, 100.0));
			return y;
		}
	}

	float BlackbodyLuminance(const float temperature)
	{
		static const double sun = SurfaceLuminance(5772.0);
		return (float)(SurfaceLuminance(temperature) / sun);
	}

	std::array<glm::vec4, BLACKBODY_TABLE_SIZE> BlackbodyTable()
	{
		std::array<glm::vec4, BLACKBODY_TABLE_SIZE> table{};
		for (size_t i = 0; i < BLACKBODY_TABLE_SIZE; i++)
		{
			const float t = (float)i / (float)(BLACKBODY_TABLE_SIZE - 1);
			table[i] = glm::vec4(BlackbodyColour(BLACKBODY_TABLE_MIN * std::pow(BLACKBODY_TABLE_MAX / BLACKBODY_TABLE_MIN, t)), 0.0f);
		}
		return table;
	}

	glm::vec3 StylisedStarColour(const float temperature)
	{
		static const glm::vec3 sun = BlackbodyColour(5772.0f);
		const glm::vec3 ratio = glm::max(BlackbodyColour(temperature) / glm::max(sun, glm::vec3(1e-6f)), glm::vec3(1e-6f));
		return glm::pow(ratio / std::max(std::max(ratio.r, ratio.g), ratio.b), glm::vec3(0.6f));
	}

	glm::vec3 BlackbodyColour(const float temperature)
	{
		// Planck's law integrated against the colour matching functions
		glm::dvec3 xyz(0.0);
		for (double wavelength = 380.0; wavelength <= 780.0; wavelength += 5.0)
			xyz += ColourMatching(wavelength) * Planck(wavelength, temperature);
		xyz /= xyz.y;

		// XYZ to linear sRGB
		const glm::dvec3 rgb(3.2406 * xyz.x - 1.5372 * xyz.y - 0.4986 * xyz.z,
							-0.9689 * xyz.x + 1.8758 * xyz.y + 0.0415 * xyz.z,
							 0.0557 * xyz.x - 0.2040 * xyz.y + 1.0570 * xyz.z);
		return glm::max(glm::vec3(rgb), glm::vec3(0.0f));
	}

	StarRenderer::StarRenderer(const SceneContext& context)
	{
		sphereMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuadsphere(1.0f, SPHERE_SUBDIVISIONS));
		quadMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuad());

		DH::GraphicsPipelineDesc starDesc;
		starDesc.vertexShader = Shaders::Create("Star.vert");
		starDesc.fragmentShader = Shaders::Create("Star.frag");
		// Positions and normals
		starDesc.vertexBindings = { sphereMesh->GetVertexBindings()[0], sphereMesh->GetVertexBindings()[1] };
		starDesc.vertexAttributes = { sphereMesh->GetVertexAttributes()[0], sphereMesh->GetVertexAttributes()[1] };
		starDesc.raster.cullMode = DH::CullMode::Back;
		starDesc.depth = { true, true, DH::CompareOp::Less };
		starDesc.blend = DH::BlendPreset::None;
		starDesc.targetLayout = context.targetLayout;
		starDesc.bindGroupLayouts = { context.frameLayout };
		starDesc.pushConstantSize = sizeof(StarPushConstants);
		starPipeline = ModalPipeline(starDesc);

		DH::GraphicsPipelineDesc coronaDesc;
		coronaDesc.vertexShader = Shaders::Create("Corona.vert");
		coronaDesc.fragmentShader = Shaders::Create("Corona.frag");
		coronaDesc.vertexBindings = quadMesh->GetVertexBindings();
		coronaDesc.vertexAttributes = quadMesh->GetVertexAttributes();
		coronaDesc.raster.cullMode = DH::CullMode::None;
		// Tested against, but not written to, the depth buffer
		coronaDesc.depth = { true, false, DH::CompareOp::Less };
		coronaDesc.blend = DH::BlendPreset::AdditiveOne;
		coronaDesc.targetLayout = context.targetLayout;
		coronaDesc.bindGroupLayouts = { context.frameLayout };
		coronaDesc.pushConstantSize = sizeof(CoronaPushConstants);
		coronaPipeline = ModalPipeline(coronaDesc);

		// Over everything, in the lens rather than the scene (occlusion is worked out on the CPU)
		DH::GraphicsPipelineDesc flareDesc;
		flareDesc.vertexShader = Shaders::Create("Flare.vert");
		flareDesc.fragmentShader = Shaders::Create("Flare.frag");
		flareDesc.vertexBindings = quadMesh->GetVertexBindings();
		flareDesc.vertexAttributes = quadMesh->GetVertexAttributes();
		flareDesc.raster.cullMode = DH::CullMode::None;
		flareDesc.depth = { false, false, DH::CompareOp::Always };
		flareDesc.blend = DH::BlendPreset::AdditiveOne;
		flareDesc.targetLayout = context.targetLayout;
		flareDesc.bindGroupLayouts = { context.frameLayout };
		flareDesc.pushConstantSize = sizeof(FlarePushConstants);
		flarePipeline = DH::GraphicsPipeline::Create(flareDesc);
	}

	glm::vec3 StarRenderer::GetSurfaceColour(const float temperature) const
	{
		return surfaceView == StarSurfaceView::Stylised ? StylisedStarColour(temperature) : BlackbodyColour(temperature);
	}

	void StarRenderer::DrawCorona(DH::CommandList& cmd, const glm::mat4& transform, const glm::vec3& colour, const float brightness) const
	{
		const CoronaPushConstants pushConstants{
			transform,
			glm::vec4(colour, hydrogenEmission),
			glm::vec4(solarFlares, surfaceView == StarSurfaceView::Stylised ? 1.0f : 0.0f, brightness, 0.0f)
		};
		cmd.PushConstants(pushConstants);
		quadMesh->Draw(cmd);
	}

	void StarRenderer::DrawSurface(DH::CommandList& cmd, const glm::mat4& transform, const glm::vec3& colour, const float temperature,
								   const float granulation, const float brightness) const
	{
		const StarPushConstants pushConstants{
			transform,
			glm::vec4(STAR_NOISE_OCTAVES, solarFlares, brightness, 0.0f),
			glm::vec4(colour, 1.0f),
			glm::vec4(temperature, granulation, sunspots, (float)surfaceView)
		};
		cmd.PushConstants(pushConstants);
		sphereMesh->Draw(cmd);
	}

	void StarRenderer::Render(DH::CommandList& cmd, const DH::PolygonMode polygonMode)
	{
		if (stars.empty()) return;

		for (const auto& [distance, index] : distanceCache)
			stars[index]->RecalculateTransformation();

		// Coronas first, so the stars are drawn over them
		cmd.BindPipeline(coronaPipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const Star& star = *stars[index];
			const glm::vec3 colour = surfaceView == StarSurfaceView::Stylised ? StylisedStarColour(star.starTemperature) : star.GetPhotosphereColour();
			DrawCorona(cmd, star.transformationMatrix, colour, 1.0f);
		}

		cmd.BindPipeline(starPipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const Star& star = *stars[index];
			const glm::vec3 colour = surfaceView == StarSurfaceView::Stylised ? StylisedStarColour(star.starTemperature) : star.GetPhotosphereColour();
			DrawSurface(cmd, star.transformationMatrix, colour, star.starTemperature, star.granulation, 1.0f);
		}
	}

	void StarRenderer::RenderCloseUp(DH::CommandList& cmd, const glm::mat4& transform, const float temperature, const float brightness)
	{
		const glm::vec3 colour = GetSurfaceColour(temperature);
		cmd.BindPipeline(coronaPipeline.Get(DH::PolygonMode::Solid));
		DrawCorona(cmd, transform, colour, brightness);
		cmd.BindPipeline(starPipeline.Get(DH::PolygonMode::Solid));
		DrawSurface(cmd, transform, colour, temperature, CLOSE_UP_GRANULATION, brightness);
	}

	float StarRenderer::GetFlareStrength(const glm::vec2& centre, const float radius, const float aspect, const float pixel) const
	{
		const float offScreen = std::max(std::abs(centre.x / aspect), std::abs(centre.y));
		return flareIntensity * SmoothStep(FLARE_START_RADIUS, FLARE_FULL_RADIUS, radius / pixel) *
			   (1.0f - SmoothStep(GLARE_FADE_START, GLARE_FADE_END, offScreen));
	}

	glm::vec3 StarRenderer::GetFlareColour(const float temperature, const glm::vec3& photosphere) const
	{
		if (surfaceView != StarSurfaceView::Stylised)
			return photosphere;
		// (The stylised colour, with a luminance of 1)
		const glm::vec3 colour = StylisedStarColour(temperature);
		return colour / std::max(glm::dot(colour, glm::vec3(0.2126f, 0.7152f, 0.0722f)), 1e-3f);
	}

	void StarRenderer::DrawFlare(DH::CommandList& cmd, const glm::vec2& centre, const float radius, const float aspect, const float pixel,
								 const float strength, const double distanceAU, const glm::vec3& colour) const
	{
		const float offScreen = std::max(std::abs(centre.x / aspect), std::abs(centre.y));
		const float size = std::clamp((float)std::pow(std::pow(std::max(distanceAU, 1e-6), -0.7), FLARE_SIZE_POWER), FLARE_MIN_SIZE, FLARE_MAX_SIZE);
		const float ghostStrength = strength * (1.0f - SmoothStep(GHOST_FADE_START, GHOST_FADE_END, offScreen));
		cmd.PushConstants(FlarePushConstants{ glm::vec4(centre, radius, aspect), glm::vec4(strength, size, pixel, ghostStrength),
											  glm::vec4(colour, 0.0f) });
		quadMesh->Draw(cmd);
	}

	void StarRenderer::RenderFlare(DH::CommandList& cmd, const glm::vec2& centre, const float radius, const float aspect, const float pixel,
								   const double distanceAU, const float temperature, const float presence) const
	{
		const float strength = presence * GetFlareStrength(centre, radius, aspect, pixel);
		if (strength <= 1e-3f)
			return;
		cmd.BindPipeline(*flarePipeline);
		DrawFlare(cmd, centre, radius, aspect, pixel, strength, distanceAU, GetFlareColour(temperature, BlackbodyColour(temperature)));
	}

	void StarRenderer::RenderFlares(DH::CommandList& cmd, const FlareView& view,
									const std::vector<sRef<CelestrialBody>>& occluders)
	{
		if (flareIntensity <= 0.0f || view.viewportSize.x <= 0.0f || view.viewportSize.y <= 0.0f)
			return;

		const float aspect = view.viewportSize.x / view.viewportSize.y;
		const float pixel = 2.0f / view.viewportSize.y; // In view units (half the viewport's height)

		// A body's disc on screen, in view units
		struct Disc
		{
			glm::vec2 centre;
			float radius;
			float depth; // Clip w
		};
		auto Project = [&](const CelestrialBody& body) {
			const glm::vec4 clip = view.viewProjection * glm::vec4(body.GetTranslation(), 1.0f);
			return Disc{ glm::vec2(clip.x / clip.w * aspect, clip.y / clip.w),
						 body.GetScale().x / clip.w * view.projectionScale, clip.w };
		};

		bool bound = false;
		for (const sRef<Star>& star : stars)
		{
			const Disc disc = Project(*star);
			if (disc.depth <= 0.0f)
				continue; // Behind the camera

			float strength = GetFlareStrength(disc.centre, disc.radius, aspect, pixel);
			if (strength <= 0.0f)
				continue;

			// Dimmed by the part of the disc hidden behind nearer bodies (at least a pixel wide, so
			// it fades smoothly as a tiny star passes behind a limb)
			const float visibleRadius = std::max(disc.radius, pixel);
			const float area = 3.14159265358979f * visibleRadius * visibleRadius;
			float hidden = 0.0f;
			for (const sRef<CelestrialBody>& occluder : occluders)
			{
				if (occluder.get() == star.get())
					continue;
				const Disc other = Project(*occluder);
				if (other.depth > 0.0f && other.depth < disc.depth)
					hidden += DiscOverlap(visibleRadius, other.radius, glm::distance(disc.centre, other.centre));
			}
			strength *= std::clamp(1.0f - hidden / area, 0.0f, 1.0f);
			if (strength <= 1e-3f)
				continue;

			if (!bound)
			{
				cmd.BindPipeline(*flarePipeline);
				bound = true;
			}
			const double distanceAU = glm::distance(star->GetTranslation(), view.cameraPosition) / Universe::AU_IN_UNITS;
			DrawFlare(cmd, disc.centre, disc.radius, aspect, pixel, strength, distanceAU,
					  GetFlareColour(star->starTemperature, star->GetPhotosphereColour()));
		}
	}
}
