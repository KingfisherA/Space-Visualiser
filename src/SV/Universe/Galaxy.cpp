#include "Galaxy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <thread>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Maths/Geometry.h"
#include "Dahlia/Platform/GPUContext.h"
#include "SV/Graphics/BloomCompositor.hpp"
#include "SV/Graphics/NoiseVolume.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "Star.hpp"

namespace SV
{
	namespace
	{
		// Mirrors GalaxyUniforms in Shaders/Include/Galaxy.glsl (std140)
		struct GalaxyUniforms
		{
			glm::vec4 centre;
			glm::vec4 bar;
			glm::vec4 arms;
			glm::vec4 disc;
			glm::vec4 glow;
			glm::vec4 dust;
			glm::vec4 bulgeColour;
			glm::vec4 discColour;
			glm::vec4 youngColour;
			glm::vec4 nebulaColour;
			glm::vec4 view;
			glm::vec4 shape;
			glm::vec4 ring;
			glm::vec4 hole;
			std::array<glm::vec4, MAX_COMPANIONS> companionCentre;
			std::array<glm::vec4, MAX_COMPANIONS> companionStretch;
			std::array<glm::vec4, MAX_COMPANIONS> companionColour;
		};

		// Mirrors CompositePushConstants in Shaders/GalaxyComposite.frag
		struct CompositePushConstants
		{
			glm::vec4 params; // x = exposure
		};

		// One star, as GalaxyStars.vert reads it (per instance)
		struct GalaxyStar
		{
			glm::vec4 star;   // xyz = position (galactic coordinates), w = luminosity
			glm::vec4 colour; // rgb = colour (linear, luminance 1), w = radius (nebulae; 0 for stars)
		};
		static_assert(sizeof(GalaxyStar) == 32);

		constexpr float PI = std::numbers::pi_v<float>;

		// The glow is smooth, so it is raymarched at this fraction of the viewport's resolution
		constexpr float GLOW_RESOLUTION = 0.5f;
		constexpr DH::Format HDR_FORMAT = DH::Format::RGBA16F;

		// The nebulae's colour: H-alpha red with a little H-beta and [O III]
		constexpr glm::vec3 NEBULA_COLOUR{ 1.0f, 0.2f, 0.4f };

		// Shares of the stars: the bar and bulge, the disc, young stars along the arms, nebulae
		// (H II regions), the companions (if any) and the rest in the halo
		constexpr float BAR_SHARE = 0.25f;
		constexpr float DISC_SHARE = 0.45f;
		constexpr float YOUNG_SHARE = 0.25f;
		constexpr float NEBULA_SHARE = 0.02f;
		constexpr float COMPANION_SHARE = 0.025f;
		// Each companion is three nested Gaussians, its halo, core and nucleus: their sizes and
		// shares of its light (mirrors CompanionLight in Shaders/Include/Galaxy.glsl)
		constexpr std::array<float, 3> COMPANION_SCALES = { 1.0f, 0.3f, 0.08f };
		constexpr std::array<float, 3> COMPANION_SHARES = { 0.6f, 0.3f, 0.1f };
		// The mean luminosity of each kind of star (at a star brightness of 1), spread log-normally
		constexpr float OLD_LUMINOSITY = 500.0f;
		constexpr float YOUNG_LUMINOSITY = 2500.0f;
		constexpr float HALO_LUMINOSITY = 300.0f;
		// Nebulae: their radii (pc, log-normal), and their luminosity per square parsec (at a
		// nebula brightness of 1), so resolved nebulae all peak at about the same brightness
		constexpr float NEBULA_RADIUS = 14.0f;
		constexpr float NEBULA_RADIUS_SPREAD = 0.5f;
		constexpr float NEBULA_SURFACE_LUMINOSITY = 8.0f;
		constexpr float LUMINOSITY_SPREAD = 0.7f; // Standard deviation of the luminosity's logarithm
		// The bar's density, exp(-(m / scale)^2) in its ellipsoidal radius m (mirrors BAR_SCALE in
		// Shaders/Include/Galaxy.glsl)
		constexpr float BAR_SCALE = 0.45f;
		// Stars are placed the same way every time
		constexpr uint32_t SEED = 1977;

		// A companion's luminosity, in the glow's units, from the brightness at its centre
		float CompanionLuminosity(const GalaxyCompanion& companion)
		{
			return companion.brightness * 2.0f * PI * companion.radius * companion.radius / COMPANION_SHARES[0];
		}

		float SmoothStep(const float edge0, const float edge1, const float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		// The arm geometry, mirroring Shaders/Include/Galaxy.glsl
		struct ArmGeometry
		{
			float angle;     // The bar's angle (radians), where the major arms start
			float tanPitch;
			float sinPitch;
			float spacing;   // Between the arms (radians)
			float radius;    // Where the major arms leave the bar
			float discRadius;
			float scaleLength;

			explicit ArmGeometry(const GalaxyParameters& p) :
				angle(glm::radians(p.barAngle)),
				tanPitch(std::tan(glm::radians(p.armPitch))),
				sinPitch(std::sin(glm::radians(p.armPitch))),
				spacing(2.0f * PI / (float)p.armCount),
				radius(p.armRadius),
				discRadius(p.discRadius),
				scaleLength(p.discScaleLength)
			{
			}

			// The arms either side of a point: a Gaussian `width` wide across each, with the minor
			// arms weighted by `minorWeight` (as Arms in the glow, with no shift)
			[[nodiscard]] float Arms(const float x, const float z, const float width, const float minorWeight) const
			{
				const float r = std::sqrt(x * x + z * z);
				const float psi = std::atan2(z, x) + std::log(std::max(r, 1.0f) / radius) / tanPitch;
				const float position = (psi - angle) / spacing;
				const float inside = std::floor(position);
				float sum = 0.0f;
				for (int side = 0; side < 2; side++)
				{
					const float across = (position - inside - (float)side) * spacing * r * sinPitch / width;
					const bool major = ((int64_t)inside + side) % 2 == 0;
					sum += std::exp(-0.5f * across * across) * (major ? 1.0f : minorWeight);
				}
				return sum;
			}

			// The angle of arm `index` at radius r
			[[nodiscard]] float ArmAngle(const int index, const float r) const
			{
				return angle + (float)index * spacing - std::log(r / radius) / tanPitch;
			}

			[[nodiscard]] float Envelope(const float r) const
			{
				return SmoothStep(0.6f * radius, radius, r) * SmoothStep(discRadius, 0.8f * discRadius, r) *
					   std::exp(-(r - radius) / (1.6f * scaleLength));
			}

			[[nodiscard]] float YoungEnvelope(const float r) const
			{
				return Envelope(r) * SmoothStep(0.7f * radius, 1.6f * radius, r);
			}

			// DiscProfile without its exponential (which the disc's radii are drawn from)
			[[nodiscard]] float DiscCutoffs(const float r) const
			{
				return (0.6f + 0.4f * SmoothStep(0.3f * radius, radius, r)) * SmoothStep(discRadius, 0.85f * discRadius, r);
			}
		};

		class Random
		{
		public:
			explicit Random(const uint32_t seed) : engine(seed) {}

			float Uniform(const float from = 0.0f, const float to = 1.0f)
			{
				return std::uniform_real_distribution<float>(from, to)(engine);
			}
			float Normal(const float deviation = 1.0f)
			{
				return std::normal_distribution<float>(0.0f, deviation)(engine);
			}
			// Gamma distributed with a whole number shape: the sum of `shape` exponentials
			float Gamma(const int shape, const float scale)
			{
				float sum = 0.0f;
				for (int i = 0; i < shape; i++)
					sum -= std::log(1.0f - Uniform());
				return sum * scale;
			}
			// Two sided exponential: exp(-|x| / scale)
			float Laplace(const float scale)
			{
				return Gamma(1, scale) * (Uniform() < 0.5f ? -1.0f : 1.0f);
			}
			glm::vec3 Direction()
			{
				const float z = Uniform(-1.0f, 1.0f);
				const float angle = Uniform(0.0f, 2.0f * PI);
				const float r = std::sqrt(std::max(1.0f - z * z, 0.0f));
				return { r * std::cos(angle), z, r * std::sin(angle) };
			}
			// Log-normal around `mean`
			float Luminosity(const float mean)
			{
				return mean * std::exp(Normal(LUMINOSITY_SPREAD) - 0.5f * LUMINOSITY_SPREAD * LUMINOSITY_SPREAD);
			}

		private:
			std::mt19937 engine;
		};

		// Star colours by temperature, as BlackbodyColour is too slow to call for every star
		class ColourTable
		{
		public:
			ColourTable()
			{
				for (size_t i = 0; i < colours.size(); i++)
					colours[i] = BlackbodyColour(TemperatureAt((float)i));
			}

			[[nodiscard]] glm::vec3 operator()(const float temperature) const
			{
				const float position = std::clamp(std::log(temperature / MIN_TEMPERATURE) / std::log(MAX_TEMPERATURE / MIN_TEMPERATURE), 0.0f, 1.0f) *
									   (float)(colours.size() - 1);
				const size_t i = std::min((size_t)position, colours.size() - 2);
				return glm::mix(colours[i], colours[i + 1], position - (float)i);
			}

		private:
			static constexpr float MIN_TEMPERATURE = 2500.0f, MAX_TEMPERATURE = 40000.0f;

			[[nodiscard]] float TemperatureAt(const float index) const
			{
				return MIN_TEMPERATURE * std::pow(MAX_TEMPERATURE / MIN_TEMPERATURE, index / (float)(colours.size() - 1));
			}

			std::array<glm::vec3, 256> colours{};
		};
	}

	GalaxyRenderer::GalaxyRenderer(const SceneContext& contextIn, const GalaxyParameters& parametersIn) :
		parameters(parametersIn),
		context(contextIn)
	{
		quadMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuad());

		const GalaxyUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(GalaxyUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);

		noiseTexture = GetNoiseVolume();
		noiseSampler = CreateNoiseSampler();

		// Set 1 of the glow and star shaders: the galaxy's parameters and the noise
		galaxyLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler3D, DH::ShaderStage::AllGraphics } });
		galaxyGroup = DH::BindGroup::Create(galaxyLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, noiseTexture, noiseSampler) });
		// Set 1 of the composite: the glow and the stars
		compositeLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::CombinedTextureSampler, DH::ShaderStage::Fragment },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler, DH::ShaderStage::Fragment } });

		DH::RenderTargetLayout hdrLayout;
		hdrLayout.colourFormats = { HDR_FORMAT };

		// The glow: written over the whole (cleared) target
		DH::GraphicsPipelineDesc glowDesc;
		glowDesc.vertexShader = Shaders::Create("GalaxyGlow.vert");
		glowDesc.fragmentShader = Shaders::Create("GalaxyGlow.frag");
		glowDesc.depth = { false, false, DH::CompareOp::Always };
		glowDesc.blend = DH::BlendPreset::None;
		glowDesc.targetLayout = hdrLayout;
		glowDesc.bindGroupLayouts = { context.frameLayout, galaxyLayout };
		glowPipeline = DH::GraphicsPipeline::Create(glowDesc);

		// Stars: quads (binding 0) per star (binding 1), adding up their light
		DH::GraphicsPipelineDesc starDesc;
		starDesc.vertexShader = Shaders::Create("GalaxyStars.vert");
		starDesc.fragmentShader = Shaders::Create("GalaxyStars.frag");
		starDesc.vertexBindings = quadMesh->GetVertexBindings();
		starDesc.vertexBindings.push_back({ 1, (uint32_t)sizeof(GalaxyStar), true });
		starDesc.vertexAttributes = quadMesh->GetVertexAttributes();
		starDesc.vertexAttributes.push_back({ 1, 1, DH::Format::RGBA32F, (uint32_t)offsetof(GalaxyStar, star) });
		starDesc.vertexAttributes.push_back({ 2, 1, DH::Format::RGBA32F, (uint32_t)offsetof(GalaxyStar, colour) });
		starDesc.raster.cullMode = DH::CullMode::None;
		starDesc.depth = { false, false, DH::CompareOp::Always };
		starDesc.blend = DH::BlendPreset::AdditiveOne;
		starDesc.targetLayout = hdrLayout;
		starDesc.bindGroupLayouts = { context.frameLayout, galaxyLayout };
		starPipeline = DH::GraphicsPipeline::Create(starDesc);

		// The composite: tone maps the two into the viewport
		DH::GraphicsPipelineDesc compositeDesc;
		compositeDesc.vertexShader = Shaders::Create("Fullscreen.vert");
		compositeDesc.fragmentShader = Shaders::Create("GalaxyComposite.frag");
		compositeDesc.depth = { false, false, DH::CompareOp::Always };
		compositeDesc.blend = DH::BlendPreset::None;
		compositeDesc.targetLayout = context.targetLayout;
		compositeDesc.bindGroupLayouts = { context.frameLayout, compositeLayout };
		compositeDesc.pushConstantSize = sizeof(CompositePushConstants);
		compositePipeline = DH::GraphicsPipeline::Create(compositeDesc);

		Regenerate();
	}

	GalaxyRenderer::~GalaxyRenderer() = default;

	void GalaxyRenderer::Regenerate()
	{
		const GalaxyParameters& p = parameters;
		const ArmGeometry arms(p);
		const ColourTable colours;
		Random random(SEED);

		const size_t total = (size_t)std::max(p.starCount, 0);
		std::vector<GalaxyStar> stars;
		stars.reserve(total);
		auto Add = [&](const glm::vec3& position, const float temperature, const float luminosity)
		{
			if (stars.size() < total)
				stars.push_back({ glm::vec4(position, luminosity), glm::vec4(colours(temperature), 0.0f) });
		};

		// The bar and bulge: old, cool stars, with a few bright red giants
		const float barCos = std::cos(arms.angle), barSin = std::sin(arms.angle);
		const size_t barCount = (size_t)(BAR_SHARE * (float)total);
		for (size_t i = 0; i < barCount; i++)
		{
			glm::vec3 position;
			if (random.Uniform() < p.barShare)
			{
				// Density exp(-(m / scale)^2): normal along each axis
				const float deviation = BAR_SCALE * std::numbers::sqrt2_v<float> * 0.5f * p.barHalfLength;
				const glm::vec3 d(random.Normal(deviation), random.Normal(deviation), random.Normal(deviation));
				const glm::vec3 local(d.x, d.y * p.barHeightRatio, d.z * p.barWidthRatio);
				position = { barCos * local.x - barSin * local.z, local.y, barSin * local.x + barCos * local.z };
			}
			else
			{
				position = random.Direction() * random.Gamma(3, p.bulgeRadius);
			}
			const bool giant = random.Uniform() < 0.03f;
			Add(position, giant ? random.Uniform(3200.0f, 4200.0f) : random.Uniform(3300.0f, 5200.0f),
				random.Luminosity(OLD_LUMINOSITY) * (giant ? 10.0f : 1.0f));
		}

		// The disc: radii from its exponential surface density, then kept in proportion to its
		// cut-offs and arms
		const size_t discCount = (size_t)(DISC_SHARE * (float)total);
		for (size_t i = 0; i < discCount;)
		{
			const float r = random.Gamma(2, p.discScaleLength);
			if (r > p.discRadius)
				continue;
			const float angle = random.Uniform(0.0f, 2.0f * PI);
			const float x = r * std::cos(angle), z = r * std::sin(angle);
			const float arm = arms.Arms(x, z, 1.6f * p.armWidth, p.minorArmStrength);
			const float weight = arms.DiscCutoffs(r) * (1.0f + arm * arms.Envelope(r)) / 2.4f;
			if (random.Uniform() >= weight)
				continue;

			const float type = random.Uniform();
			const float temperature = type < 0.7f ? random.Uniform(3400.0f, 5300.0f) :
									  type < 0.95f ? random.Uniform(5300.0f, 7500.0f) : random.Uniform(7500.0f, 10000.0f);
			Add({ x, random.Laplace(p.discScaleHeight), z }, temperature, random.Luminosity(OLD_LUMINOSITY));
			i++;
		}

		// Young stars and nebulae: the ring's share of them (the rest are along the arms), and a
		// place on the ring
		const float ringShare = p.ringStrength / (1.0f + p.ringStrength);
		auto OnRing = [&]()
		{
			const float r = p.ringRadius + random.Normal(p.ringWidth);
			const float angle = random.Uniform(0.0f, 2.0f * PI);
			return glm::vec3(r * std::cos(angle), 0.0f, r * std::sin(angle));
		};

		// Young stars along the arms (and ring), in clusters and associations: hot and bright,
		// with a few red supergiants
		const size_t youngEnd = stars.size() + (size_t)(YOUNG_SHARE * (float)total);
		while (stars.size() < youngEnd)
		{
			glm::vec3 centre;
			if (random.Uniform() < ringShare)
			{
				centre = OnRing();
			}
			else
			{
				const float r = random.Uniform(0.6f * p.armRadius, p.discRadius);
				const int arm = std::min((int)(random.Uniform() * (float)p.armCount), p.armCount - 1);
				// As in the glow, the minor arms have fewer
				if (random.Uniform() >= arms.YoungEnvelope(r) * (arm % 2 == 0 ? 1.0f : p.minorArmStrength))
					continue;
				const float angle = arms.ArmAngle(arm, r);
				const float across = random.Normal(p.armWidth);
				centre = { (r + across) * std::cos(angle), 0.0f, (r + across) * std::sin(angle) };
			}
			centre.y = random.Laplace(0.5f * p.youngScaleHeight);

			const int members = 1 + (int)random.Gamma(1, 25.0f);
			const float size = random.Uniform(10.0f, 60.0f);
			for (int m = 0; m < members && stars.size() < youngEnd; m++)
			{
				const glm::vec3 position = centre + glm::vec3(random.Normal(size), random.Normal(0.6f * size), random.Normal(size));
				if (random.Uniform() < 0.03f)
				{
					Add(position, random.Uniform(3300.0f, 4000.0f), random.Luminosity(YOUNG_LUMINOSITY) * 5.0f);
				}
				else
				{
					const float u = random.Uniform();
					Add(position, 8000.0f * std::pow(35000.0f / 8000.0f, u * u), random.Luminosity(YOUNG_LUMINOSITY));
				}
			}
		}

		// Nebulae (H II regions): the gas lit by the youngest clusters, in complexes along the inner
		// edges of the arms (just past the dust lanes), with a few between them
		const glm::vec3 nebulaColour = NEBULA_COLOUR / glm::dot(NEBULA_COLOUR, glm::vec3(0.2126f, 0.7152f, 0.0722f));
		const size_t nebulaEnd = stars.size() + (size_t)(NEBULA_SHARE * (float)total);
		while (stars.size() < nebulaEnd)
		{
			glm::vec3 centre;
			const float place = random.Uniform();
			if (place < ringShare)
			{
				centre = OnRing();
			}
			else if (place < ringShare + 0.85f * (1.0f - ringShare))
			{
				const float r = random.Uniform(0.6f * p.armRadius, p.discRadius);
				if (random.Uniform() >= arms.Envelope(r))
					continue;
				const int arm = std::min((int)(random.Uniform() * (float)p.armCount), p.armCount - 1);
				const float angle = arms.ArmAngle(arm, r);
				const float across = random.Normal(0.3f * p.armWidth) - 0.15f * p.armWidth;
				centre = { (r + across) * std::cos(angle), 0.0f, (r + across) * std::sin(angle) };
			}
			else
			{
				const float r = random.Gamma(2, p.discScaleLength);
				if (r < 0.6f * p.armRadius || r > p.discRadius)
					continue;
				const float angle = random.Uniform(0.0f, 2.0f * PI);
				centre = { r * std::cos(angle), 0.0f, r * std::sin(angle) };
			}
			centre.y = random.Laplace(0.3f * p.youngScaleHeight);

			const int members = 1 + (int)random.Gamma(1, 1.5f);
			for (int m = 0; m < members && stars.size() < nebulaEnd; m++)
			{
				const glm::vec3 position = centre + (m == 0 ? glm::vec3(0.0f) : glm::vec3(random.Normal(50.0f), random.Normal(10.0f), random.Normal(50.0f)));
				const float radius = NEBULA_RADIUS * std::exp(random.Normal(NEBULA_RADIUS_SPREAD));
				const float luminosity = NEBULA_SURFACE_LUMINOSITY * radius * radius * std::exp(random.Normal(0.6f));
				stars.push_back({ glm::vec4(position, luminosity), glm::vec4(nebulaColour, radius) });
			}
		}

		// The companions' stars, in proportion to their light, placed as their glow is
		float companionLight = 0.0f;
		const size_t companionCount = std::min(p.companions.size(), MAX_COMPANIONS);
		for (size_t c = 0; c < companionCount; c++)
			companionLight += CompanionLuminosity(p.companions[c]);
		const size_t companionStars = companionLight > 0.0f ? (size_t)(COMPANION_SHARE * (float)total) : 0;
		for (size_t c = 0; c < companionCount; c++)
		{
			const GalaxyCompanion& companion = p.companions[c];
			const size_t count = (size_t)((float)companionStars * CompanionLuminosity(companion) / companionLight);
			for (size_t i = 0; i < count; i++)
			{
				// As in the glow, the core and nucleus are weighted by its concentration
				const float core = companion.concentration * COMPANION_SHARES[1], nucleus = companion.concentration * COMPANION_SHARES[2];
				const float pick = random.Uniform(0.0f, COMPANION_SHARES[0] + core + nucleus);
				const size_t k = pick < COMPANION_SHARES[0] ? 0 : pick < COMPANION_SHARES[0] + core ? 1 : 2;
				const float sigma = companion.radius * COMPANION_SCALES[k];
				const glm::vec3 position = companion.position + glm::vec3(random.Normal(sigma), random.Normal(sigma), random.Normal(sigma)) +
										   companion.stretch * COMPANION_SCALES[k] * random.Normal();
				Add(position, companion.temperature + random.Normal(500.0f), random.Luminosity(OLD_LUMINOSITY));
			}
		}

		// The halo: old stars in a flattened sphere
		while (stars.size() < total)
		{
			const float r = std::min(3000.0f * std::exp(random.Normal(0.7f)), 2.0f * p.discRadius);
			glm::vec3 position = random.Direction() * r;
			position.y *= 0.7f;
			Add(position, random.Uniform(4200.0f, 5600.0f), random.Luminosity(HALO_LUMINOSITY));
		}

		// The previous stars may still be drawing
		if (starBuffer != nullptr)
			DH::GPUContext::Get()->WaitIdle();
		starCount = stars.size();
		starBuffer = starCount > 0 ? DH::Buffer::Create({ starCount * sizeof(GalaxyStar), DH::BufferUsage::Vertex, DH::BufferMemory::Static }, stars.data())
								   : nullptr;
	}

	void GalaxyRenderer::EnsureTargets(const size_t width, const size_t height)
	{
		if (starTarget != nullptr && starTarget->GetWidth() == width && starTarget->GetHeight() == height)
			return;
		// The previous targets may still be in use
		if (starTarget != nullptr)
			DH::GPUContext::Get()->WaitIdle();

		auto CreateTarget = [](size_t targetWidth, size_t targetHeight)
		{
			return DH::Framebuffer::Create(DH::FramebufferSpecification{ std::max<size_t>(targetWidth, 1), std::max<size_t>(targetHeight, 1), 1,
																		 { DH::FramebufferAttachment(HDR_FORMAT) } });
		};
		glowTarget = CreateTarget((size_t)std::ceil((float)width * GLOW_RESOLUTION), (size_t)std::ceil((float)height * GLOW_RESOLUTION));
		starTarget = CreateTarget(width, height);
		compositeGroup = DH::BindGroup::Create(compositeLayout, {
			DH::BindGroupEntry::TextureSampler(0, glowTarget->GetAttachmentTexture(0), context.clampSampler),
			DH::BindGroupEntry::TextureSampler(1, starTarget->GetAttachmentTexture(0), context.clampSampler) });
	}

	void GalaxyRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const GalaxyView& view)
	{
		RenderLayers(cmd, target.GetWidth(), target.GetHeight(), view);

		// Both, tone mapped into the target
		DH::RenderPassDesc pass;
		pass.target = &target;
		pass.clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*compositePipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *compositeGroup);
		cmd.PushConstants(CompositePushConstants{ glm::vec4(parameters.brightness, 0.0f, 0.0f, 0.0f) });
		cmd.Draw(3);
		cmd.EndRenderPass();
	}

	void GalaxyRenderer::AddTo(DH::CommandList& cmd, const float scale)
	{
		if (addPipeline == nullptr)
		{
			DH::GraphicsPipelineDesc addDesc;
			addDesc.vertexShader = Shaders::Create("Fullscreen.vert");
			addDesc.fragmentShader = Shaders::Create("GalaxyComposite.frag");
			addDesc.depth = { false, false, DH::CompareOp::Always };
			addDesc.blend = DH::BlendPreset::AdditiveOne;
			addDesc.targetLayout = BloomCompositor::GetSceneLayout();
			addDesc.bindGroupLayouts = { context.frameLayout, compositeLayout };
			addDesc.pushConstantSize = sizeof(CompositePushConstants);
			addPipeline = DH::GraphicsPipeline::Create(addDesc);
		}
		cmd.BindPipeline(*addPipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *compositeGroup);
		cmd.PushConstants(CompositePushConstants{ glm::vec4(scale, 1.0f, 0.0f, 0.0f) });
		cmd.Draw(3);
	}

	void GalaxyRenderer::RenderLayers(DH::CommandList& cmd, const size_t width, const size_t height, const GalaxyView& view)
	{
		EnsureTargets(width, height);

		const GalaxyParameters& p = parameters;
		const float luminance = glm::dot(NEBULA_COLOUR, glm::vec3(0.2126f, 0.7152f, 0.0722f));
		GalaxyUniforms uniforms{
			glm::vec4(view.centre, p.discRadius),
			glm::vec4(glm::radians(p.barAngle), p.barHalfLength, p.barWidthRatio, p.barHeightRatio),
			glm::vec4((float)p.armCount, std::tan(glm::radians(p.armPitch)), p.armRadius, p.armWidth),
			glm::vec4(p.discScaleLength, p.discScaleHeight, p.youngScaleHeight, p.dustScaleHeight),
			glm::vec4(p.bulgeGlow, p.discGlow, p.armGlow, p.nebulaGlow),
			glm::vec4(p.dustDepth, p.dustLaneOffset, p.bulgeRadius, 0.0f),
			glm::vec4(BlackbodyColour(p.bulgeTemperature), 0.0f),
			glm::vec4(BlackbodyColour(p.discTemperature), 0.0f),
			glm::vec4(BlackbodyColour(p.youngTemperature), 0.0f),
			glm::vec4(NEBULA_COLOUR / luminance, 0.0f),
			glm::vec4(view.pixelAngle, p.starBrightness, p.nebulaGlow, 0.0f),
			glm::vec4(p.minorArmStrength, p.barShare, 0.0f, 0.0f),
			glm::vec4(p.ringRadius, p.ringWidth, p.ringStrength, 0.0f),
			glm::vec4(view.holeCentre, view.holeRadius),
			{}, {}, {}
		};
		const size_t companionCount = std::min(p.companions.size(), MAX_COMPANIONS);
		uniforms.shape.z = (float)companionCount;
		for (size_t c = 0; c < companionCount; c++)
		{
			const GalaxyCompanion& companion = p.companions[c];
			uniforms.companionCentre[c] = glm::vec4(companion.position, companion.radius);
			uniforms.companionStretch[c] = glm::vec4(companion.stretch, CompanionLuminosity(companion) * p.companionGlow);
			uniforms.companionColour[c] = glm::vec4(BlackbodyColour(companion.temperature), companion.concentration);
		}
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;

		// The glow
		pass.target = glowTarget.get();
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*glowPipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *galaxyGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		// The stars
		pass.target = starTarget.get();
		cmd.BeginRenderPass(pass);
		if (starBuffer != nullptr)
		{
			cmd.BindPipeline(*starPipeline);
			cmd.BindGroup(0, *context.frameGroup);
			cmd.BindGroup(1, *galaxyGroup);
			cmd.BindVertexBuffer(1, *starBuffer);
			quadMesh->Draw(cmd, (uint32_t)starCount);
		}
		cmd.EndRenderPass();
	}

	namespace
	{
		GalaxyPreset MilkyWay()
		{
			GalaxyPreset preset;
			preset.name = "Milky Way";
			// The Sun: 8.2 kpc from the centre, 20 pc above the plane (between the Sagittarius-Carina
			// and Perseus arms). Seen face-on from the north galactic pole with the Sun below the
			// centre, as in the usual maps of the Milky Way.
			preset.markers = { { "Galactic centre", glm::vec3(0.0f) }, { "Sun", glm::vec3(8200.0f, 20.0f, 0.0f) } };
			preset.overviewPitch = 90.0f;
			preset.overviewYaw = -90.0f;
			preset.overviewZoom = 40000.0f;
			return preset;
		}

		// M51: a grand design spiral, its two arms equally strong and no real bar, with the
		// companion NGC 5195 just past the tip of one arm, behind the disc (so the arm's dust
		// crosses it), and a stream of stars pulled out between them. Seen nearly face-on with the
		// companion at the top, as it usually is.
		GalaxyPreset Whirlpool()
		{
			GalaxyPreset preset;
			preset.name = "Whirlpool (M51)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 0.0f;
			p.barHalfLength = 1500.0f;
			p.barWidthRatio = 0.5f;
			p.barHeightRatio = 0.4f;
			p.bulgeRadius = 300.0f;
			p.barShare = 0.15f;
			p.armCount = 2;
			p.armPitch = 18.0f;
			p.armRadius = 1500.0f;
			p.armWidth = 550.0f;
			p.minorArmStrength = 1.0f;
			p.discScaleLength = 2800.0f;
			p.discScaleHeight = 250.0f;
			p.discRadius = 13000.0f;
			p.dustLaneOffset = 220.0f;
			p.bulgeGlow = 3.0f;
			p.discGlow = 0.6f;
			p.armGlow = 2.0f;
			p.nebulaGlow = 1.0f;
			p.dustDepth = 5.0f;
			p.bulgeTemperature = 4400.0f;
			p.discTemperature = 5000.0f;
			p.youngTemperature = 18000.0f;

			// The arm's tip at 10 kpc (arm 1 at that radius), and the companion
			const glm::vec3 tip(-9020.0f, 0.0f, -4316.0f);
			const glm::vec3 companion(-11500.0f, -1800.0f, -160.0f);
			p.companions = {
				{ "NGC 5195", companion, 1200.0f, glm::vec3(300.0f, 0.0f, 400.0f), 1.0f, 4300.0f, 1.0f },
				{ "", 0.5f * (tip + companion), 450.0f, 0.3f * (companion - tip), 0.15f, 9000.0f, 0.0f },
			};
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 68.0f;
			preset.overviewYaw = -90.0f;
			preset.overviewZoom = 52000.0f;
			preset.fieldOfView = 30.0f;
			return preset;
		}

		// M31: larger than the Milky Way, with a big bulge, a dusty ring of star formation at 10 kpc
		// and tightly wound arms, and its companions M32 (small and dense, just off the disc) and
		// M110 (larger, fainter and drawn out). Seen 77 degrees from face-on, as it is from Earth,
		// through a narrow field of view (from far away, so with little perspective).
		GalaxyPreset Andromeda()
		{
			GalaxyPreset preset;
			preset.name = "Andromeda (M31)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 30.0f;
			p.barHalfLength = 4000.0f;
			p.barWidthRatio = 0.45f;
			p.barHeightRatio = 0.45f;
			p.bulgeRadius = 900.0f;
			p.barShare = 0.3f;
			p.armCount = 2;
			p.armPitch = 7.0f;
			p.armRadius = 5000.0f;
			p.armWidth = 600.0f;
			p.minorArmStrength = 0.7f;
			p.ringRadius = 10000.0f;
			p.ringWidth = 1100.0f;
			p.ringStrength = 0.8f;
			p.discScaleLength = 4500.0f;
			p.discScaleHeight = 400.0f;
			p.discRadius = 22000.0f;
			p.youngScaleHeight = 100.0f;
			p.dustScaleHeight = 150.0f;
			p.dustLaneOffset = 300.0f;
			p.bulgeGlow = 7.0f;
			p.discGlow = 1.1f;
			p.armGlow = 0.2f;
			p.nebulaGlow = 0.4f;
			p.dustDepth = 5.0f;
			p.bulgeTemperature = 4300.0f;
			p.discTemperature = 4800.0f;
			p.youngTemperature = 14000.0f;
			p.starCount = 300000;
			p.brightness = 0.6f;
			p.companions = {
				{ "M32", glm::vec3(1500.0f, 600.0f, 22000.0f), 220.0f, glm::vec3(60.0f, 0.0f, 0.0f), 2.0f, 4400.0f, 1.0f },
				{ "M110", glm::vec3(-3000.0f, 8000.0f, -1500.0f), 700.0f, glm::vec3(1100.0f, 350.0f, 0.0f), 0.3f, 4600.0f, 0.15f },
			};
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 13.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 180000.0f;
			preset.fieldOfView = 15.0f;
			return preset;
		}

		// Arp 273: UGC 1810, a large spiral pulled by the close passage of its smaller companion UGC
		// 1813 into a rose: tightly wound arms, studded with pink nebulae and blue clusters, and an
		// outer arm looping round it like a ring. UGC 1813 lies below it, nearly edge-on, bright with
		// young stars at its centre, joined to UGC 1810 by a faint bridge of stars pulled out between them.
		GalaxyPreset RoseGalaxy()
		{
			GalaxyPreset preset;
			preset.name = "Rose Galaxy (Arp 273)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 20.0f;
			p.barHalfLength = 1800.0f;
			p.barWidthRatio = 0.55f;
			p.barHeightRatio = 0.45f;
			p.bulgeRadius = 600.0f;
			p.barShare = 0.2f;
			p.armCount = 2;
			p.armPitch = 11.0f;
			p.armRadius = 2500.0f;
			p.armWidth = 650.0f;
			p.minorArmStrength = 0.8f;
			p.ringRadius = 15000.0f;
			p.ringWidth = 1400.0f;
			p.ringStrength = 1.0f;
			p.discScaleLength = 5000.0f;
			p.discScaleHeight = 300.0f;
			p.discRadius = 21000.0f;
			p.youngScaleHeight = 90.0f;
			p.dustScaleHeight = 110.0f;
			p.dustLaneOffset = 280.0f;
			p.bulgeGlow = 4.0f;
			p.discGlow = 0.7f;
			p.armGlow = 2.0f;
			p.nebulaGlow = 2.0f;
			p.dustDepth = 1.5f;
			p.bulgeTemperature = 4500.0f;
			p.discTemperature = 5200.0f;
			p.youngTemperature = 19000.0f;
			p.starCount = 300000;

			// UGC 1813 (in front of and below UGC 1810, from the first view), drawn out along its
			// disc, and the bridge of stars between them
			const glm::vec3 companion(6000.0f, -5000.0f, 29000.0f);
			const glm::vec3 edge(3000.0f, 0.0f, 17000.0f);
			p.companions = {
				{ "UGC 1813", companion, 1000.0f, glm::vec3(3500.0f, 300.0f, -800.0f), 0.7f, 7000.0f, 1.2f },
				{ "", 0.5f * (edge + companion), 1100.0f, 0.35f * (companion - edge), 0.12f, 9000.0f, 0.0f },
			};
			preset.markers = { { "UGC 1810", glm::vec3(0.0f) } };
			preset.overviewPitch = 55.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 160000.0f;
			preset.fieldOfView = 30.0f;
			return preset;
		}

		// M101: a large, face-on spiral (about 50 kpc across) with a small bulge and many loosely
		// wound arms, crowded with giant H II regions
		GalaxyPreset Pinwheel()
		{
			GalaxyPreset preset;
			preset.name = "Pinwheel (M101)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 0.0f;
			p.barHalfLength = 1200.0f;
			p.barWidthRatio = 0.6f;
			p.barHeightRatio = 0.5f;
			p.bulgeRadius = 350.0f;
			p.barShare = 0.1f;
			p.armCount = 4;
			p.armPitch = 24.0f;
			p.armRadius = 2000.0f;
			p.armWidth = 750.0f;
			p.minorArmStrength = 0.8f;
			p.discScaleLength = 6000.0f;
			p.discScaleHeight = 300.0f;
			p.discRadius = 26000.0f;
			p.dustLaneOffset = 250.0f;
			p.bulgeGlow = 2.0f;
			p.discGlow = 0.5f;
			p.armGlow = 2.2f;
			p.nebulaGlow = 1.8f;
			p.dustDepth = 3.0f;
			p.bulgeTemperature = 4600.0f;
			p.discTemperature = 5300.0f;
			p.youngTemperature = 20000.0f;
			p.starCount = 350000;
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 90.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 125000.0f;
			preset.fieldOfView = 30.0f;
			return preset;
		}

		// NGC 1300: the archetypal barred spiral. A long bar, with dust lanes along its leading
		// edges, and two grand design arms sweeping from its ends half way round the galaxy.
		GalaxyPreset Ngc1300()
		{
			GalaxyPreset preset;
			preset.name = "NGC 1300";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 35.0f;
			p.barHalfLength = 6500.0f;
			p.barWidthRatio = 0.2f;
			p.barHeightRatio = 0.2f;
			p.bulgeRadius = 500.0f;
			p.barShare = 0.85f;
			p.armCount = 2;
			p.armPitch = 20.0f;
			p.armRadius = 6500.0f;
			p.armWidth = 750.0f;
			p.minorArmStrength = 0.1f;
			p.discScaleLength = 5000.0f;
			p.discScaleHeight = 300.0f;
			p.discRadius = 17000.0f;
			p.dustLaneOffset = 250.0f;
			p.bulgeGlow = 4.0f;
			p.discGlow = 0.35f;
			p.armGlow = 3.0f;
			p.nebulaGlow = 1.0f;
			p.dustDepth = 3.0f;
			p.bulgeTemperature = 4400.0f;
			p.discTemperature = 5000.0f;
			p.youngTemperature = 17000.0f;
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 55.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 75000.0f;
			preset.fieldOfView = 30.0f;
			return preset;
		}

		// M33: a small (18 kpc), nearly bulgeless spiral with patchy, loosely wound arms and bright H
		// II regions, seen 54 degrees from face-on
		GalaxyPreset Triangulum()
		{
			GalaxyPreset preset;
			preset.name = "Triangulum (M33)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 0.0f;
			p.barHalfLength = 500.0f;
			p.barWidthRatio = 0.6f;
			p.barHeightRatio = 0.5f;
			p.bulgeRadius = 150.0f;
			p.barShare = 0.0f;
			p.armCount = 2;
			p.armPitch = 26.0f;
			p.armRadius = 900.0f;
			p.armWidth = 650.0f;
			p.minorArmStrength = 0.7f;
			p.discScaleLength = 1800.0f;
			p.discScaleHeight = 250.0f;
			p.discRadius = 9500.0f;
			p.youngScaleHeight = 100.0f;
			p.dustScaleHeight = 120.0f;
			p.dustLaneOffset = 150.0f;
			p.bulgeGlow = 1.2f;
			p.discGlow = 1.0f;
			p.armGlow = 1.6f;
			p.nebulaGlow = 2.2f;
			p.dustDepth = 2.0f;
			p.bulgeTemperature = 4800.0f;
			p.discTemperature = 5600.0f;
			p.youngTemperature = 20000.0f;
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 36.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 42000.0f;
			preset.fieldOfView = 30.0f;
			return preset;
		}

		// M104: a huge, bright bulge of old stars, cut across by a ring of dust in its thin disc,
		// seen just 6 degrees from edge-on: the brim of the hat
		GalaxyPreset Sombrero()
		{
			GalaxyPreset preset;
			preset.name = "Sombrero (M104)";
			GalaxyParameters& p = preset.parameters;
			p.barAngle = 0.0f;
			p.barHalfLength = 1500.0f;
			p.barWidthRatio = 0.8f;
			p.barHeightRatio = 0.7f;
			p.bulgeRadius = 1300.0f;
			p.barShare = 0.0f;
			p.armCount = 2;
			p.armPitch = 6.0f;
			p.armRadius = 5000.0f;
			p.armWidth = 500.0f;
			p.minorArmStrength = 0.5f;
			p.ringRadius = 8500.0f;
			p.ringWidth = 900.0f;
			p.ringStrength = 1.5f;
			p.discScaleLength = 5000.0f;
			p.discScaleHeight = 250.0f;
			p.discRadius = 15000.0f;
			p.youngScaleHeight = 80.0f;
			p.dustScaleHeight = 150.0f;
			p.dustLaneOffset = 200.0f;
			p.bulgeGlow = 10.0f;
			p.discGlow = 0.9f;
			p.armGlow = 0.15f;
			p.nebulaGlow = 0.1f;
			p.dustDepth = 8.0f;
			p.bulgeTemperature = 4300.0f;
			p.discTemperature = 4700.0f;
			p.youngTemperature = 9000.0f;
			p.brightness = 0.5f;
			preset.markers = { { "Nucleus", glm::vec3(0.0f) } };
			preset.overviewPitch = 6.0f;
			preset.overviewYaw = 0.0f;
			preset.overviewZoom = 65000.0f;
			preset.fieldOfView = 25.0f;
			return preset;
		}
	}

	const GalaxyPreset& GetGalaxyPreset(const GalaxyType type)
	{
		static const std::array<GalaxyPreset, (size_t)GalaxyType::Count> presets = { MilkyWay(), Whirlpool(), Andromeda(), RoseGalaxy(),
																								 Pinwheel(), Ngc1300(), Triangulum(), Sombrero() };
		return presets[std::min((size_t)type, presets.size() - 1)];
	}
}
