#include "UniverseRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Maths/Geometry.h"
#include "SV/Graphics/NoiseVolume.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "SV/Universe/Coordinates.hpp"
#include "SV/Universe/CompactObject.hpp"
#include "SV/Universe/Galaxy.hpp"
#include "SV/Universe/Nebula.hpp"
#include "SV/Universe/Star.hpp"

namespace SV
{
	namespace
	{
		// Mirrors UniverseUniforms in Shaders/Include/Universe.glsl (std140)
		struct UniverseUniforms
		{
			glm::mat4 viewProjection;
			glm::mat4 view;
			glm::vec4 cameraHigh;
			glm::vec4 cameraLow;
			glm::vec4 viewport; // x = pixel angle, y = width, z = height, w = time
			glm::vec4 stars;    // x = star brightness, y = gain for stars, z = zoom (pc), w = the Sun's visibility
			glm::vec4 extended; // x = galaxy surface gain, y = gain for unresolved galaxies, z = nebula surface gain, w = nebula markers
			glm::vec4 markers;  // x = neutron star markers, y = black hole markers, z = stars shown, w = exposure
			// The galaxy, nebula and point shown close up (xyz = their positions' high parts), and
			// how much of each of their sprites or markers is still drawn (w)
			glm::vec4 hiddenGalaxy;
			glm::vec4 hiddenNebula;
			glm::vec4 hiddenPoint;
			glm::vec4 hiddenMilkyWay; // The Milky Way's sprite, while the galaxy model draws it
			// The nebula close up's region, whose other sprites are hidden as it fades in (they are
			// parts of it the close-up draws): x = its radius across the line of sight from the Sun,
			// y = its depth along it (pc)
			glm::vec4 hiddenNebulaRegion;
		};

		// Mirrors UniversePushConstants in Shaders/Include/Universe.glsl
		struct UniversePushConstants
		{
			glm::vec4 pass;
		};

		// Mirrors PASS_* in Shaders/Include/Universe.glsl
		enum class Pass
		{
			Points,
			GalaxyPoints,
			GalaxyDiscs,
			GalaxySpheroids,
			NebulaPoints,
			NebulaSprites
		};

		// Mirrors KIND_* in Shaders/UniversePoints.vert
		enum class PointKind
		{
			Star,
			Pulsar,
			MillisecondPulsar,
			Magnetar,
			NeutronStar,
			XrayBinary,
			StellarBlackHole,
			SupermassiveBlackHole,
			Sun
		};

		// A star, neutron star or black hole, as UniversePoints.vert reads it (per instance)
		struct PointInstance
		{
			glm::vec4 high;   // xyz = position (high part), w = luminosity
			glm::vec4 low;    // xyz = position (low part), w = kind
			glm::vec4 colour; // rgb = colour (luminance 1), w = spin period
		};

		// A galaxy or nebula, as UniverseSprites.vert reads it (per instance)
		struct SpriteInstance
		{
			glm::vec4 high;   // xyz = position (high part), w = radius
			glm::vec4 low;    // xyz = position (low part), w = shape
			glm::vec4 major;  // xyz = major axis, w = axis ratio
			glm::vec4 normal; // xyz = the disc's normal, w = T-type
			glm::vec4 colour; // rgb = colour, w = luminosity
			glm::vec4 extra;  // x = seed, y = the bulge's share of the light
		};

		// A position as a float and the remainder
		void Split(const glm::dvec3& position, glm::vec4& high, glm::vec4& low)
		{
			const glm::vec3 h(position);
			high = glm::vec4(h, high.w);
			low = glm::vec4(glm::vec3(position - glm::dvec3(h)), low.w);
		}

		// The markers' colours: pulsars blue, millisecond pulsars violet, magnetars magenta, isolated
		// neutron stars white, X-ray binaries orange and black holes (their rings) gold
		glm::vec3 MarkerColour(const CatalogueKind kind)
		{
			switch (kind)
			{
			case CatalogueKind::Pulsar: return { 0.45f, 0.75f, 1.0f };
			case CatalogueKind::MillisecondPulsar: return { 0.7f, 0.6f, 1.0f };
			case CatalogueKind::Magnetar: return { 1.0f, 0.35f, 0.85f };
			case CatalogueKind::NeutronStar: return { 0.9f, 0.95f, 1.0f };
			case CatalogueKind::XrayBinary: return { 1.0f, 0.65f, 0.3f };
			default: return { 1.0f, 0.72f, 0.35f };
			}
		}

		PointKind PointKindOf(const CompactSource& source)
		{
			switch (source.kind)
			{
			case CatalogueKind::Pulsar: return PointKind::Pulsar;
			case CatalogueKind::MillisecondPulsar: return PointKind::MillisecondPulsar;
			case CatalogueKind::Magnetar: return PointKind::Magnetar;
			case CatalogueKind::NeutronStar: return PointKind::NeutronStar;
			case CatalogueKind::XrayBinary: return PointKind::XrayBinary;
			default: return source.name == "Sagittarius A*" ? PointKind::SupermassiveBlackHole : PointKind::StellarBlackHole;
			}
		}

		// The share of a galaxy's light in its bulge, by its T-type (de Vaucouleurs)
		float BulgeShare(const ExtendedObject& galaxy)
		{
			if (galaxy.shape == SpriteShape::EllipticalGalaxy)
				return 1.0f;
			if (galaxy.shape == SpriteShape::IrregularGalaxy)
				return 0.0f;
			return std::clamp(0.45f - 0.06f * galaxy.typeCode, 0.03f, 0.5f);
		}

		SpriteInstance MakeSprite(const ExtendedObject& object)
		{
			SpriteInstance sprite{};
			sprite.high.w = object.radius;
			sprite.low.w = (float)object.shape;
			Split(object.position, sprite.high, sprite.low);
			sprite.major = glm::vec4(object.majorAxis, object.axisRatio);
			sprite.normal = glm::vec4(object.normal, object.typeCode);
			sprite.colour = glm::vec4(BlackbodyColour(object.temperature), object.luminosity);
			sprite.extra = glm::vec4(object.seed, object.kind == CatalogueKind::Galaxy ? BulgeShare(object) : 0.0f, 0.0f, 0.0f);
			return sprite;
		}

		// Galaxies' and nebulae's sprites are lit by these gains on their surface brightness (in
		// reference fluxes per steradian), so they show at the stars' exposure (the eye sees them
		// much fainter)
		constexpr float GALAXY_SURFACE_GAIN = 400.0f;
		constexpr float NEBULA_SURFACE_GAIN = 40.0f;
		// Galaxies too small to see as sprites are raised by the square of the zoom past this (pc),
		// so the far ones still show as the camera zooms out
		constexpr double GALAXY_GAIN_ZOOM = 2.0e7;
		constexpr double MAX_GALAXY_GAIN = 1.0e4;
		// Likewise stars past this zoom (pc), up to a limit
		constexpr double STAR_GAIN_ZOOM = 40.0;
		constexpr double MAX_STAR_GAIN = 2000.0;
		// The stars fade out as the camera leaves them this far behind (pc): they are only the Sun's
		// neighbourhood, where the Milky Way's own stars take over
		constexpr double STAR_FADE_START = 2000.0;
		constexpr double STAR_FADE_END = 6000.0;
		// And the galaxy model's stars are cleared around the Sun, where the real stars are
		constexpr float LOCAL_HOLE_RADIUS = 700.0f;
		// Neutron stars and nebulae are marked as the zoom passes these (pc), until the Milky Way is
		// too small to see them apart
		constexpr double MARKER_ZOOM_START = 5.0;
		constexpr double MARKER_ZOOM_END = 60.0;
		constexpr double MARKER_FADE_START = 1.0e5;
		constexpr double MARKER_FADE_END = 4.0e5;
		constexpr double NEBULA_MARKER_ZOOM_START = 300.0;
		constexpr double NEBULA_MARKER_ZOOM_END = 2000.0;
		// Marker brightnesses, in reference fluxes
		constexpr float NEUTRON_STAR_MARKER = 0.1f;
		constexpr float BLACK_HOLE_MARKER = 0.06f;
		constexpr float NEBULA_MARKER = 0.02f;
		// The Milky Way's glow relative to the galaxy test's exposure (GalaxyParameters::brightness).
		// Seen from inside, the diffuse glow of its unresolved stars would be a haze over the whole
		// sky at an exposure for its stars, as the eye's adaptation never lets it be: it is dimmed
		// to INSIDE_GLOW until the zoom passes the disc's thickness.
		constexpr float MILKY_WAY_SCALE = 1.0f;
		constexpr float INSIDE_GLOW = 0.05f;
		constexpr double INSIDE_GLOW_ZOOM_START = 500.0;
		constexpr double INSIDE_GLOW_ZOOM_END = 6000.0;
		// The pulsars' time is kept within this (s), so it stays precise in a float
		constexpr double TIME_WRAP = 3600.0;

		// The Milky Way's radius (pc): the galaxy model draws it while it is larger than the first
		// size on screen (pixels, as a radius), fading out to its sprite below it
		constexpr double MILKY_WAY_RADIUS = 15000.0;
		constexpr double MILKY_WAY_SPRITE_START = 8.0;
		constexpr double MILKY_WAY_SPRITE_END = 30.0;

		// Close-ups fade in over their sprites as these grow from the first size (pixels, as a
		// radius) to the second
		constexpr float GALAXY_CLOSE_UP_START = 70.0f;
		constexpr float GALAXY_CLOSE_UP_END = 180.0f;
		constexpr float NEBULA_CLOSE_UP_START = 60.0f;
		constexpr float NEBULA_CLOSE_UP_END = 160.0f;
		// The nebula sprites hidden by a nebula close-up: within this many of its model's largest
		// semi-axis across the line of sight from the Sun, and this share of its distance along it
		// (the catalogues' distances for a complex such as Orion's differ by tens of parsecs)
		constexpr float NEBULA_HIDE_RADIUS = 1.6f;
		constexpr double NEBULA_HIDE_DEPTH = 0.25;
		constexpr double NEBULA_HIDE_MIN_DEPTH = 30.0;
		// And black holes and neutron stars as the zoom closes from the first to the second (in
		// their Schwarzschild radii), when the camera is centred within COMPACT_REACH of one
		constexpr double COMPACT_CLOSE_UP_START = 1000.0;
		constexpr double COMPACT_CLOSE_UP_END = 500.0;
		constexpr double COMPACT_REACH = 50.0;
		// The closest zoom to them, and the zoom "Go to" ends at (Schwarzschild radii)
		constexpr double BLACK_HOLE_MIN_ZOOM = 2.2;
		constexpr double NEUTRON_STAR_MIN_ZOOM = 4.0;
		constexpr double BLACK_HOLE_VIEW_ZOOM = 22.0;
		constexpr double NEUTRON_STAR_VIEW_ZOOM = 45.0;
		// Schwarzschild radius (pc) per solar mass, and the masses taken for stellar black holes
		// and neutron stars whose masses aren't known
		constexpr double SCHWARZSCHILD_PC_PER_SOLAR_MASS = 2953.25 / 3.0856776e16;
		constexpr double STELLAR_BLACK_HOLE_MASS = 10.0;
		constexpr double NEUTRON_STAR_MASS = 1.4;

		// The objects shown close up by the test scenes' presets, by designation
		constexpr std::pair<const char*, GalaxyType> GALAXY_FEATURES[] = {
			{ "M31", GalaxyType::Andromeda }, { "M33", GalaxyType::Triangulum }, { "M51", GalaxyType::Whirlpool },
			{ "M101", GalaxyType::Pinwheel }, { "M104", GalaxyType::Sombrero }, { "NGC1300", GalaxyType::Ngc1300 },
			{ "UGC1810", GalaxyType::RoseGalaxy }, { "NGC6946", GalaxyType::Fireworks }, { "M63", GalaxyType::Sunflower },
			{ "NGC3115", GalaxyType::Spindle }, { "M64", GalaxyType::BlackEye }, { "M81", GalaxyType::Bodes },
			{ "NGC4565", GalaxyType::Needle }, { "NGC253", GalaxyType::Sculptor }, { "NGC1316", GalaxyType::FornaxA },
			{ "M82", GalaxyType::Cigar }, { "ESO350-40", GalaxyType::Cartwheel }, { "NGC4151", GalaxyType::EyeOfSauron },
			{ "NGC7742", GalaxyType::FriedEgg }, { "NGC6822", GalaxyType::Barnards } };
		constexpr std::pair<const char*, NebulaType> NEBULA_FEATURES[] = {
			{ "M1", NebulaType::Crab }, { "M42", NebulaType::Orion }, { "M16", NebulaType::PillarsOfCreation },
			{ "NGC7635", NebulaType::Bubble }, { "NGC6905", NebulaType::BlueFlash }, { "NGC6309", NebulaType::Box },
			{ "NGC6960", NebulaType::Veil }, { "NGC7000", NebulaType::NorthAmerica }, { "NGC3372", NebulaType::Carina },
			{ "NGC6188", NebulaType::Rim }, { "NGC2238", NebulaType::Rosette }, { "IC405", NebulaType::FlamingStar }, { "M8", NebulaType::Lagoon } };

		// Whether a designation ("M31 / NGC 224, ...") includes an id ("M31"), ignoring spaces
		bool HasDesignation(const std::string& designation, const char* id)
		{
			std::string compact;
			for (const char c : designation)
			{
				if (c != ' ')
					compact += c;
			}
			const std::string wanted(id);
			for (size_t start = 0; start < compact.size();)
			{
				size_t end = compact.find_first_of("/,", start);
				if (end == std::string::npos)
					end = compact.size();
				if (compact.compare(start, end - start, wanted) == 0)
					return true;
				start = end + 1;
			}
			return false;
		}

		// The turn from a preset's model axes to the world's that shows the object from the Sun as
		// the preset's first view (pitch and yaw, as Camera3D) shows it, north up
		glm::dmat3 PresetToWorld(const glm::dvec3& position, const float pitch, const float yaw)
		{
			const glm::dvec3 forward = glm::normalize(position);
			const glm::dvec3 north = Universe::EquatorialDirectionToWorld(0.0, 90.0);
			glm::dvec3 up = north - glm::dot(north, forward) * forward;
			up = glm::length(up) > 1e-6 ? glm::normalize(up) : glm::dvec3(0.0, 1.0, 0.0);
			const glm::dvec3 right = glm::cross(forward, up);
			// World to the Sun's camera (rows right, up, back), and the preset's model to its camera
			const glm::dmat3 skyToWorld(right, up, -forward);
			const glm::dmat3 modelToCamera(glm::rotate(glm::radians((double)pitch), glm::dvec3(1.0, 0.0, 0.0)) *
										   glm::rotate(glm::radians((double)yaw), glm::dvec3(0.0, 1.0, 0.0)));
			return skyToWorld * modelToCamera;
		}

		double SmoothStep(const double edge0, const double edge1, const double x)
		{
			const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
			return t * t * (3.0 - 2.0 * t);
		}

		// The Milky Way is drawn relative to the camera's centre in parsecs, so the galaxy's model
		// stays in floats wherever the camera is
		constexpr float GALAXY_NEAR_FRACTION = 1e-3f;
		constexpr float GALAXY_EXTENT = 60000.0f;
	}

	UniverseRenderer::UniverseRenderer(const SceneContext& contextIn, const Catalogue& catalogue) :
		context(contextIn),
		compositor(contextIn)
	{
		quadMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuad());

		// The points: stars, then compact sources, then the Sun
		std::vector<PointInstance> points;
		points.reserve(catalogue.stars.size() + catalogue.compactSources.size() + 1);
		for (const CatalogueStar& star : catalogue.stars)
		{
			PointInstance point{ glm::vec4(0.0f, 0.0f, 0.0f, star.luminosity), glm::vec4(0.0f, 0.0f, 0.0f, (float)PointKind::Star),
								 glm::vec4(BlackbodyColour(star.temperature), 0.0f) };
			Split(star.position, point.high, point.low);
			points.push_back(point);
		}
		starCount = catalogue.stars.size();
		for (const CompactSource& source : catalogue.compactSources)
		{
			PointInstance point{ glm::vec4(0.0f), glm::vec4(0.0f, 0.0f, 0.0f, (float)PointKindOf(source)), glm::vec4(MarkerColour(source.kind), source.period) };
			Split(source.position, point.high, point.low);
			points.push_back(point);
		}
		compactCount = catalogue.compactSources.size();
		points.push_back({ glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, (float)PointKind::Sun), glm::vec4(BlackbodyColour(5772.0f), 0.0f) });
		pointBuffer = DH::Buffer::Create({ points.size() * sizeof(PointInstance), DH::BufferUsage::Vertex, DH::BufferMemory::Static }, points.data());

		auto CreateSprites = [](const std::vector<ExtendedObject>& objects, size_t& count) -> sRef<DH::Buffer>
		{
			std::vector<SpriteInstance> sprites;
			sprites.reserve(objects.size());
			for (const ExtendedObject& object : objects)
				sprites.push_back(MakeSprite(object));
			count = sprites.size();
			return count > 0 ? DH::Buffer::Create({ count * sizeof(SpriteInstance), DH::BufferUsage::Vertex, DH::BufferMemory::Static }, sprites.data()) : nullptr;
		};
		galaxyBuffer = CreateSprites(catalogue.galaxies, galaxyCount);
		nebulaBuffer = CreateSprites(catalogue.nebulae, nebulaCount);

		// Set 0: the uniforms and the noise (for the sprites' patterns)
		const UniverseUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(UniverseUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);
		noiseTexture = GetNoiseVolume();
		noiseSampler = CreateNoiseSampler();
		universeLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics },
			{ 1, DH::ShaderResourceType::CombinedTextureSampler3D, DH::ShaderStage::AllGraphics } });
		universeGroup = DH::BindGroup::Create(universeLayout, {
			DH::BindGroupEntry::UniformBuffer(0, uniformBuffer),
			DH::BindGroupEntry::TextureSampler(1, noiseTexture, noiseSampler) });

		// Quads (binding 0) per instance (binding 1), adding up their light
		auto CreatePipeline = [&](const char* vertex, const char* fragment, const uint32_t stride, const std::vector<uint32_t>& offsets)
		{
			DH::GraphicsPipelineDesc desc;
			desc.vertexShader = Shaders::Create(vertex);
			desc.fragmentShader = Shaders::Create(fragment);
			desc.vertexBindings = quadMesh->GetVertexBindings();
			desc.vertexBindings.push_back({ 1, stride, true });
			desc.vertexAttributes = quadMesh->GetVertexAttributes();
			for (size_t i = 0; i < offsets.size(); i++)
				desc.vertexAttributes.push_back({ (uint32_t)(i + 1), 1, DH::Format::RGBA32F, offsets[i] });
			desc.raster.cullMode = DH::CullMode::None;
			desc.depth = { false, false, DH::CompareOp::Always };
			desc.blend = DH::BlendPreset::AdditiveOne;
			desc.targetLayout = BloomCompositor::GetSceneLayout();
			desc.bindGroupLayouts = { universeLayout };
			desc.pushConstantSize = sizeof(UniversePushConstants);
			return DH::GraphicsPipeline::Create(desc);
		};
		pointPipeline = CreatePipeline("UniversePoints.vert", "UniversePoints.frag", sizeof(PointInstance),
									   { offsetof(PointInstance, high), offsetof(PointInstance, low), offsetof(PointInstance, colour) });
		spritePipeline = CreatePipeline("UniverseSprites.vert", "UniverseSprites.frag", sizeof(SpriteInstance),
										{ offsetof(SpriteInstance, high), offsetof(SpriteInstance, low), offsetof(SpriteInstance, major),
										  offsetof(SpriteInstance, normal), offsetof(SpriteInstance, colour), offsetof(SpriteInstance, extra) });

		// The Milky Way: the galaxy test's model, seen through its own camera
		const FrameUniforms frame{};
		galaxyFrameBuffer = DH::Buffer::Create({ sizeof(FrameUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &frame);
		galaxyFrameGroup = DH::BindGroup::Create(context.frameLayout, { DH::BindGroupEntry::UniformBuffer(0, galaxyFrameBuffer) });
		SceneContext galaxyContext = context;
		galaxyContext.frameGroup = galaxyFrameGroup;
		milkyWay = umake<GalaxyRenderer>(galaxyContext, GetGalaxyPreset(GalaxyType::MilkyWay).parameters);
		galaxyToWorld = GetGalacticModelToWorld();

		// Close-ups: the galaxies and nebulae with presets, and the black holes and neutron stars
		for (const auto& [id, type] : GALAXY_FEATURES)
		{
			for (const ExtendedObject& galaxy : catalogue.galaxies)
			{
				if (!HasDesignation(galaxy.designation, id))
					continue;
				const GalaxyPreset& preset = GetGalaxyPreset(type);
				galaxyFeatures.push_back({ galaxy.position, galaxy.radius, (int)type, PresetToWorld(galaxy.position, preset.overviewPitch, preset.overviewYaw) });
				break;
			}
		}
		for (const auto& [id, type] : NEBULA_FEATURES)
		{
			for (const ExtendedObject& nebula : catalogue.nebulae)
			{
				if (!HasDesignation(nebula.designation, id))
					continue;
				const NebulaPreset& preset = GetNebulaPreset(type);
				const glm::vec3& size = preset.parameters.size;
				Feature feature{ nebula.position, nebula.radius, (int)type, PresetToWorld(nebula.position, preset.overviewPitch, preset.overviewYaw) };
				feature.hideRadius = NEBULA_HIDE_RADIUS * std::max({ size.x, size.y, size.z });
				feature.hideDepth = std::max(NEBULA_HIDE_DEPTH * glm::length(nebula.position), NEBULA_HIDE_MIN_DEPTH);
				nebulaFeatures.push_back(feature);
				break;
			}
		}
		for (const CompactSource& source : catalogue.compactSources)
		{
			const bool blackHole = source.kind == CatalogueKind::BlackHole;
			const double mass = !blackHole ? NEUTRON_STAR_MASS : source.name == "Sagittarius A*" ? SGR_A_MASS : STELLAR_BLACK_HOLE_MASS;
			compactPoints.push_back({ source.position, mass * SCHWARZSCHILD_PC_PER_SOLAR_MASS, blackHole });
		}
		featuredGalaxyFrame = CreateLocalFrame();
		featuredNebulaFrame = CreateLocalFrame();
		compactFrame = CreateLocalFrame();
	}

	UniverseRenderer::LocalFrame UniverseRenderer::CreateLocalFrame() const
	{
		const FrameUniforms frame{};
		LocalFrame local;
		local.buffer = DH::Buffer::Create({ sizeof(FrameUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &frame);
		local.group = DH::BindGroup::Create(context.frameLayout, { DH::BindGroupEntry::UniformBuffer(0, local.buffer) });
		return local;
	}

	void UniverseRenderer::UpdateLocalFrame(const LocalFrame& frame, const UniverseView& view, const glm::dmat3& modelToWorld, const glm::dvec3& centre,
											const double unit, const float extent, const float aspect) const
	{
		// The camera in the model's axes and units, from its offset from the render origin (precise
		// even for neutron stars kiloparsecs from the Sun)
		const glm::dmat3 worldToModel = glm::transpose(modelToWorld);
		const glm::vec3 camera(worldToModel * ((view.origin - centre) + view.offset) / unit);
		glm::mat4 viewMatrix(view.rotation * glm::mat3(modelToWorld));
		viewMatrix = viewMatrix * glm::translate(glm::mat4(1.0f), -camera);
		const float distance = glm::length(camera);
		const glm::mat4 projection = glm::perspective(view.fieldOfView, aspect, std::max(1e-3f * distance, 1e-6f), distance + 4.0f * extent);

		FrameUniforms uniforms{};
		uniforms.viewProjection = projection * viewMatrix;
		uniforms.view = viewMatrix;
		uniforms.projection = projection;
		uniforms.cameraPosition = glm::vec4(camera, 1.0f);
		uniforms.cameraCentre = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		frame.buffer->Update(&uniforms, sizeof(uniforms));
	}

	const UniverseRenderer::CompactPoint* UniverseRenderer::FindCompactAt(const glm::dvec3& centre) const
	{
		const CompactPoint* nearest = nullptr;
		double nearestDistance = 0.0;
		for (const CompactPoint& point : compactPoints)
		{
			const double distance = glm::distance(point.position, centre) / point.schwarzschildRadius;
			if (distance < COMPACT_REACH && (nearest == nullptr || distance < nearestDistance))
			{
				nearest = &point;
				nearestDistance = distance;
			}
		}
		return nearest;
	}

	const UniverseRenderer::Feature* UniverseRenderer::FindFeature(const std::vector<Feature>& features, const UniverseView& view,
																   const float pixelAngle, float& radiusPixels)
	{
		const Feature* largest = nullptr;
		radiusPixels = 0.0f;
		for (const Feature& feature : features)
		{
			const glm::dvec3 relative = feature.position - view.camera;
			const double distance = std::max(glm::length(relative), 1e-9);
			// (In front of the camera, or around it)
			if (glm::dot(glm::dvec3(view.rotation * glm::vec3(relative / distance)), glm::dvec3(0.0, 0.0, -1.0)) < 0.0 && distance > feature.radius)
				continue;
			const float pixels = (float)(feature.radius / distance / pixelAngle);
			if (pixels > radiusPixels)
			{
				radiusPixels = pixels;
				largest = &feature;
			}
		}
		return largest;
	}

	bool UniverseRenderer::IsInNebulaRegion(const Feature& nebula, const glm::dvec3& position)
	{
		// Mirrors ShownNebula in Shaders/Include/Universe.glsl
		const glm::dvec3 direction = glm::normalize(nebula.position);
		const glm::dvec3 relative = position - nebula.position;
		const double along = glm::dot(relative, direction);
		return glm::length(relative - along * direction) <= nebula.hideRadius && std::abs(along) <= nebula.hideDepth;
	}

	double UniverseRenderer::GetMinZoom(const glm::dvec3& centre) const
	{
		const CompactPoint* point = showCloseUps ? FindCompactAt(centre) : nullptr;
		if (point == nullptr)
			return 0.0;
		return (point->blackHole ? BLACK_HOLE_MIN_ZOOM : NEUTRON_STAR_MIN_ZOOM) * point->schwarzschildRadius;
	}

	double UniverseRenderer::GetLandmarkZoom(const CatalogueLandmark& landmark) const
	{
		const CompactPoint* point = showCloseUps && landmark.radius <= 0.0f && landmark.kind != CatalogueKind::Star ? FindCompactAt(landmark.position) : nullptr;
		if (point == nullptr)
			return 0.0;
		return (point->blackHole ? BLACK_HOLE_VIEW_ZOOM : NEUTRON_STAR_VIEW_ZOOM) * point->schwarzschildRadius;
	}

	UniverseRenderer::~UniverseRenderer() = default;

	float UniverseRenderer::GetProminence(const CatalogueLandmark& landmark, const UniverseView& view, const float pixelAngle, const bool picking) const
	{
		// The light (after the exposure, as the shaders' points) and size (pixels) to be labelled,
		// or picked: about the faintest point that shows, and a sprite a few pixels across
		const float LABEL_LIGHT = picking ? 0.02f : 0.5f;
		const float LABEL_RADIUS = picking ? 1.5f : 4.0f;
		const double zoom = view.zoom;
		const double distance = std::max(glm::distance(landmark.position, view.camera), 1e-9);
		const float radiusPixels = (float)(landmark.radius / distance / pixelAngle);
		switch (landmark.kind)
		{
		case CatalogueKind::Star:
		{
			if (!showStars)
				return -1.0f;
			const double gain = zoomGain ? std::min(std::pow(std::max(zoom / STAR_GAIN_ZOOM, 1.0), 2.0), MAX_STAR_GAIN) : 1.0;
			const double shown = 1.0 - SmoothStep(STAR_FADE_START, STAR_FADE_END, glm::length(view.camera));
			const double light = landmark.luminosity / (distance * distance) / 0.855 * starBrightness * gain * shown * exposure;
			if (light < LABEL_LIGHT)
				return -1.0f;
			// Those known only by a designation after those with proper names (their priority is
			// lowered for it: see LoadStars in Catalogue.cpp)
			const float designated = landmark.priority - std::log10(std::max(landmark.luminosity, 1e-4f));
			return std::max((float)std::log10(light / LABEL_LIGHT) + 0.01f + 0.5f * designated, 0.0f);
		}
		case CatalogueKind::Galaxy:
		{
			if (!showGalaxies)
				return -1.0f;
			const double gain = zoomGain ? std::min(std::pow(std::max(zoom / GALAXY_GAIN_ZOOM, 1.0), 2.0), MAX_GALAXY_GAIN) : 1.0;
			const double light = landmark.luminosity / (distance * distance) / 0.855 * GALAXY_SURFACE_GAIN * galaxyBrightness * gain * exposure;
			// Large enough to see, or bright and well known (with a common name: see the priority in
			// Catalogue.cpp's FinishGalaxy)
			const bool wellKnown = landmark.priority - (std::log10(std::max(landmark.luminosity, 1.0f)) - 6.0f) >= 2.5f;
			// (For picking, any that shows, and the well known ones however faint)
			if (radiusPixels < LABEL_RADIUS && (picking ? light < LABEL_LIGHT && !wellKnown : light < LABEL_LIGHT || !wellKnown))
				return -1.0f;
			return landmark.priority + (float)std::log10(std::max(light, 1e-3)) * 0.5f + std::log10(std::max(radiusPixels, 0.1f));
		}
		case CatalogueKind::PlanetaryNebula:
		case CatalogueKind::HIIRegion:
		case CatalogueKind::SupernovaRemnant:
		{
			if (!showNebulae)
				return -1.0f;
			// Parts of the nebula shown close up (other than itself) are drawn by it, and left unlabelled
			if (hiddenNebula != nullptr && hiddenNebulaPresence > 0.5f && IsInNebulaRegion(*hiddenNebula, landmark.position) &&
				glm::distance(landmark.position, hiddenNebula->position) > 1e-3)
				return -1.0f;
			// Large enough to see, or a well known one marked while the galaxy is in view
			// (Any marked one, for picking)
			const bool marked = (landmark.priority >= 2.0f || picking) && zoom >= NEBULA_MARKER_ZOOM_START && (distance < 3.0 * zoom || picking);
			// For picking, also one bright enough to show as a point, and the well known ones (with
			// common names: see the priority in Catalogue.cpp's LoadNebulae) wherever they are: from
			// the solar system most are far too small to see, but where they are is known
			const bool known = landmark.priority - std::log10(std::max(landmark.radius, 1e-6f)) >= 2.5f;
			const double light = landmark.luminosity / (distance * distance) / 0.855 * NEBULA_SURFACE_GAIN * nebulaBrightness * exposure;
			if (radiusPixels < LABEL_RADIUS && !marked && !(picking && (known || light >= LABEL_LIGHT)))
				return -1.0f;
			return landmark.priority + std::log10(std::max(radiusPixels, 0.1f));
		}
		default:
		{
			const bool blackHole = landmark.kind == CatalogueKind::BlackHole;
			// Picked wherever their markers show, and the notable ones (Sagittarius A*, the stellar
			// black holes, and the pulsars with names or in supernova remnants: see their priorities in
			// Catalogue.cpp) at any closer zoom too, such as from the solar system
			const bool notable = picking && landmark.priority >= -1.5f;
			if (!(blackHole ? showBlackHoles : showNeutronStars) || zoom > MARKER_FADE_START ||
				(zoom < (picking ? MARKER_ZOOM_START : MARKER_ZOOM_END) && !notable))
				return -1.0f;
			// Sagittarius A* while the galaxy is in view; the others when near the camera's centre
			const double reach = landmark.priority >= 10.0f ? 100.0 : landmark.priority >= -1.0f ? 2.0 : 0.7;
			if (distance > reach * zoom && !picking)
				return -1.0f;
			const float prominence = landmark.priority - (float)std::log10(distance / zoom);
			return picking ? std::max(prominence, 0.0f) : prominence;
		}
		}
	}

	void UniverseRenderer::RenderMilkyWay(DH::CommandList& cmd, const DH::Framebuffer& scene, const UniverseView& view, const float pixelAngle)
	{
		// The camera in the galaxy's axes, in parsecs from the render origin
		const glm::dmat3 worldToGalaxy = glm::transpose(galaxyToWorld);
		const glm::vec3 camera(worldToGalaxy * (view.camera - view.origin));
		const glm::vec3 centre(worldToGalaxy * (GetGalacticCentre() - view.origin));
		const glm::mat3 rotation = view.rotation * glm::mat3(galaxyToWorld);
		glm::mat4 viewMatrix(rotation);
		viewMatrix = viewMatrix * glm::translate(glm::mat4(1.0f), -camera);

		const float aspect = (float)scene.GetWidth() / (float)std::max<size_t>(scene.GetHeight(), 1);
		const float nearPlane = std::max(GALAXY_NEAR_FRACTION * view.zoom, 1e-6f);
		const float farPlane = glm::length(camera - centre) + GALAXY_EXTENT;
		const glm::mat4 projection = glm::perspective(view.fieldOfView, aspect, nearPlane, farPlane);

		FrameUniforms frame{};
		frame.viewProjection = projection * viewMatrix;
		frame.view = viewMatrix;
		frame.projection = projection;
		frame.cameraPosition = glm::vec4(camera, 1.0f);
		frame.cameraCentre = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		galaxyFrameBuffer->Update(&frame, sizeof(frame));

		// The model's own stars are cleared around the Sun while the camera is near enough to see
		// the real ones
		const double sunDistance = glm::length(view.camera);
		GalaxyView galaxyView;
		galaxyView.centre = centre;
		galaxyView.pixelAngle = pixelAngle;
		galaxyView.holeCentre = glm::vec3(worldToGalaxy * -GetGalacticCentre());
		galaxyView.holeRadius = LOCAL_HOLE_RADIUS * (float)(1.0 - SmoothStep(STAR_FADE_START, STAR_FADE_END, sunDistance));
		milkyWay->RenderLayers(cmd, scene.GetWidth(), scene.GetHeight(), galaxyView);
	}

	void UniverseRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const UniverseView& view)
	{
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), 1.0f);
		const float aspect = (float)scene.GetWidth() / (float)std::max<size_t>(scene.GetHeight(), 1);
		const float pixelAngle = 2.0f * std::tan(0.5f * view.fieldOfView) / (float)std::max<size_t>(scene.GetHeight(), 1);

		// The close-ups shown, and how far each has faded in
		float galaxyPixels = 0.0f, nebulaPixels = 0.0f;
		const Feature* galaxy = showCloseUps && showGalaxies ? FindFeature(galaxyFeatures, view, pixelAngle, galaxyPixels) : nullptr;
		const Feature* nebula = showCloseUps && showNebulae ? FindFeature(nebulaFeatures, view, pixelAngle, nebulaPixels) : nullptr;
		CloseUps closeUps;
		closeUps.galaxy = galaxy;
		hiddenNebula = nebula;
		closeUps.galaxyPresence = galaxy != nullptr ? (float)SmoothStep(GALAXY_CLOSE_UP_START, GALAXY_CLOSE_UP_END, galaxyPixels) : 0.0f;
		closeUps.nebula = nebula;
		closeUps.nebulaPresence = nebula != nullptr ? (float)SmoothStep(NEBULA_CLOSE_UP_START, NEBULA_CLOSE_UP_END, nebulaPixels) : 0.0f;
		hiddenNebulaPresence = closeUps.nebulaPresence;
		const CompactPoint* compact = showCloseUps ? FindCompactAt(view.origin) : nullptr;
		if (compact != nullptr && !(compact->blackHole ? showBlackHoles : showNeutronStars))
			compact = nullptr;
		closeUps.compact = compact;
		closeUps.compactPresence = compact != nullptr
									   ? (float)SmoothStep(COMPACT_CLOSE_UP_START, COMPACT_CLOSE_UP_END, view.zoom / compact->schwarzschildRadius)
									   : 0.0f;

		// The universe (unless a black hole or neutron star close up hides all of it)
		if (closeUps.compactPresence < 1.0f)
		{
			RenderScene(cmd, scene, view, pixelAngle, aspect, closeUps);
			compositor.Composite(cmd, target, exposure, bloom);
		}

		// The nebula's light over it (its own background stars left out: the universe has them)
		if (closeUps.nebulaPresence > 0.0f && closeUps.compactPresence < 1.0f)
		{
			if (featuredNebula == nullptr || featuredNebulaPreset != nebula->preset)
			{
				SceneContext nebulaContext = context;
				nebulaContext.frameGroup = featuredNebulaFrame.group;
				featuredNebula = umake<NebulaRenderer>(nebulaContext, GetNebulaPreset((NebulaType)nebula->preset).parameters);
				featuredNebulaPreset = nebula->preset;
			}
			featuredNebula->parameters.starFieldBrightness = 0.0f;
			featuredNebula->compositeMode = CompositeMode::Add;
			featuredNebula->opacity = closeUps.nebulaPresence * nebulaBrightness;
			UpdateLocalFrame(featuredNebulaFrame, view, nebula->modelToWorld, nebula->position, 1.0, 3.0f * nebula->radius, aspect);
			featuredNebula->Update(view.deltaTime);
			featuredNebula->Render(cmd, target, NebulaView{ glm::vec3(0.0f), pixelAngle });
		}

		// The black hole or neutron star, fading in over everything (with its own sky, bent by it)
		if (closeUps.compactPresence > 0.0f)
		{
			if (compactCloseUp == nullptr || compactCloseUpIsBlackHole != compact->blackHole)
			{
				SceneContext compactContext = context;
				compactContext.frameGroup = compactFrame.group;
				compactCloseUp = umake<CompactObjectRenderer>(compactContext, compact->blackHole ? GetBlackHolePreset() : GetPulsarPreset());
				compactCloseUpIsBlackHole = compact->blackHole;
			}
			compactCloseUp->compositeMode = closeUps.compactPresence < 1.0f ? CompositeMode::Blend : CompositeMode::Replace;
			compactCloseUp->opacity = closeUps.compactPresence;
			// Centred on it exactly (its position is only as precise as a double far from the Sun)
			const glm::dvec3 centre = glm::distance(view.origin, compact->position) < 5.0 * compact->schwarzschildRadius ? view.origin : compact->position;
			UpdateLocalFrame(compactFrame, view, galaxyToWorld, centre, compact->schwarzschildRadius, 2000.0f, aspect);
			compactCloseUp->Update(view.deltaTime);
			compactCloseUp->Render(cmd, target, CompactObjectView{ glm::vec3(0.0f), pixelAngle });
		}
	}

	void UniverseRenderer::RenderScene(DH::CommandList& cmd, DH::Framebuffer& scene, const UniverseView& view, const float pixelAngle,
									   const float aspect, const CloseUps& closeUps)
	{
		const float width = (float)scene.GetWidth(), height = (float)std::max<size_t>(scene.GetHeight(), 1);
		const glm::mat4 viewMatrix(view.rotation);
		const glm::mat4 projection = glm::perspective(view.fieldOfView, aspect, 1.0f, 2.0f);
		const double zoom = view.zoom;
		const double sunDistance = glm::length(view.camera);
		const float starGain = zoomGain ? (float)std::min(std::pow(std::max(zoom / STAR_GAIN_ZOOM, 1.0), 2.0), MAX_STAR_GAIN) : 1.0f;
		const float galaxyGain = zoomGain ? (float)std::min(std::pow(std::max(zoom / GALAXY_GAIN_ZOOM, 1.0), 2.0), MAX_GALAXY_GAIN) : 1.0f;
		const float starsShown = showStars ? (float)(1.0 - SmoothStep(STAR_FADE_START, STAR_FADE_END, sunDistance)) : 0.0f;
		const float fade = (float)(1.0 - SmoothStep(MARKER_FADE_START, MARKER_FADE_END, zoom));
		const float markers = (float)SmoothStep(MARKER_ZOOM_START, MARKER_ZOOM_END, zoom) * fade * markerBrightness;
		const float nebulaMarkers = (float)SmoothStep(NEBULA_MARKER_ZOOM_START, NEBULA_MARKER_ZOOM_END, zoom) * fade * markerBrightness * NEBULA_MARKER;

		UniverseUniforms uniforms{};
		uniforms.viewProjection = projection * viewMatrix;
		uniforms.view = viewMatrix;
		Split(view.camera, uniforms.cameraHigh, uniforms.cameraLow);
		uniforms.viewport = glm::vec4(pixelAngle, width, height, (float)std::fmod(view.time, TIME_WRAP));
		uniforms.stars = glm::vec4(starBrightness, starGain, (float)zoom, view.sunVisibility * (showStars ? 1.0f : 0.0f));
		uniforms.extended = glm::vec4(GALAXY_SURFACE_GAIN * galaxyBrightness, galaxyGain, NEBULA_SURFACE_GAIN * nebulaBrightness, nebulaMarkers);
		uniforms.markers = glm::vec4(showNeutronStars ? NEUTRON_STAR_MARKER * markers : 0.0f, showBlackHoles ? BLACK_HOLE_MARKER * markers : 0.0f,
									 starsShown, exposure);
		// The sprites and markers of what is shown close up fade out as it fades in
		// (Faster, so the last of a sprite, which may be brighter than the close-up's edge, is gone
		// as it finishes)
		auto Hidden = [](const glm::dvec3& position, const float presence)
		{
			const float shown = std::max(1.0f - 1.5f * presence, 0.0f);
			return glm::vec4(glm::vec3(position), shown * shown);
		};
		uniforms.hiddenGalaxy = closeUps.galaxy != nullptr ? Hidden(closeUps.galaxy->position, closeUps.galaxyPresence) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		uniforms.hiddenNebula = closeUps.nebula != nullptr ? Hidden(closeUps.nebula->position, closeUps.nebulaPresence) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		uniforms.hiddenPoint = closeUps.compact != nullptr ? Hidden(closeUps.compact->position, closeUps.compactPresence) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		// The Milky Way: the galaxy model while it is large enough on screen, otherwise its sprite
		// (the model's stars would pile up in a few pixels)
		const double milkyWayPixels = MILKY_WAY_RADIUS / std::max(glm::distance(view.camera, GetGalacticCentre()), 1.0) / pixelAngle;
		const float milkyWayPresence = showMilkyWay ? (float)SmoothStep(MILKY_WAY_SPRITE_START, MILKY_WAY_SPRITE_END, milkyWayPixels) : 0.0f;
		uniforms.hiddenMilkyWay = Hidden(GetGalacticCentre(), milkyWayPresence);
		uniforms.hiddenNebulaRegion = closeUps.nebula != nullptr ? glm::vec4(closeUps.nebula->hideRadius, (float)closeUps.nebula->hideDepth, 0.0f, 0.0f)
																 : glm::vec4(0.0f);
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		if (milkyWayPresence > 0.0f)
			RenderMilkyWay(cmd, scene, view, pixelAngle);

		// The galaxy close up: the galaxy model with its preset, turned as it is seen from the Sun
		if (closeUps.galaxyPresence > 0.0f)
		{
			const Feature& galaxy = *closeUps.galaxy;
			if (featuredGalaxy == nullptr || featuredGalaxyPreset != galaxy.preset)
			{
				SceneContext galaxyContext = context;
				galaxyContext.frameGroup = featuredGalaxyFrame.group;
				featuredGalaxy = umake<GalaxyRenderer>(galaxyContext, GetGalaxyPreset((GalaxyType)galaxy.preset).parameters);
				featuredGalaxyPreset = galaxy.preset;
			}
			UpdateLocalFrame(featuredGalaxyFrame, view, galaxy.modelToWorld, galaxy.position, 1.0, GALAXY_EXTENT, aspect);
			GalaxyView galaxyView;
			galaxyView.pixelAngle = pixelAngle;
			featuredGalaxy->RenderLayers(cmd, scene.GetWidth(), scene.GetHeight(), galaxyView);
		}

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);

		if (milkyWayPresence > 0.0f)
		{
			const float inside = (float)(INSIDE_GLOW + (1.0 - INSIDE_GLOW) * SmoothStep(INSIDE_GLOW_ZOOM_START, INSIDE_GLOW_ZOOM_END, zoom));
			milkyWay->AddTo(cmd, milkyWayPresence * inside * milkyWayBrightness * MILKY_WAY_SCALE * milkyWay->parameters.brightness / std::max(exposure, 1e-3f));
		}
		if (closeUps.galaxyPresence > 0.0f)
			featuredGalaxy->AddTo(cmd, closeUps.galaxyPresence * galaxyBrightness * featuredGalaxy->parameters.brightness / std::max(exposure, 1e-3f));

		auto Draw = [&](DH::GraphicsPipeline& pipeline, DH::Buffer& instances, const size_t count, const Pass which)
		{
			if (count == 0)
				return;
			cmd.BindPipeline(pipeline);
			cmd.BindGroup(0, *universeGroup);
			cmd.BindVertexBuffer(1, instances);
			cmd.PushConstants(UniversePushConstants{ glm::vec4((float)which, 0.0f, 0.0f, 0.0f) });
			quadMesh->Draw(cmd, (uint32_t)count);
		};
		if (showGalaxies && galaxyBuffer != nullptr)
		{
			Draw(*spritePipeline, *galaxyBuffer, galaxyCount, Pass::GalaxyDiscs);
			Draw(*spritePipeline, *galaxyBuffer, galaxyCount, Pass::GalaxySpheroids);
			Draw(*spritePipeline, *galaxyBuffer, galaxyCount, Pass::GalaxyPoints);
		}
		if (showNebulae && nebulaBuffer != nullptr)
		{
			Draw(*spritePipeline, *nebulaBuffer, nebulaCount, Pass::NebulaSprites);
			Draw(*spritePipeline, *nebulaBuffer, nebulaCount, Pass::NebulaPoints);
		}
		// Stars (and the Sun) and the compact sources
		Draw(*pointPipeline, *pointBuffer, starCount + compactCount + 1, Pass::Points);
		cmd.EndRenderPass();
	}
}
