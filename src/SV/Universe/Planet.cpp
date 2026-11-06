#include "Planet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Maths/Geometry.h"
#include "SV/Universe/Rings.hpp"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"

namespace SV
{
#pragma region Planets

	namespace
	{
		// Earth's surface map turns this much per unit of planet animation time; the other planets
		// turn in proportion to their rotation rates (negative for retrograde rotation)
		constexpr float EARTH_ROTATION_SPEED = 0.3f;
		constexpr float EARTH_DAY_HOURS = 23.934f;

		float RotationSpeed(const float periodHours)
		{
			return EARTH_ROTATION_SPEED * EARTH_DAY_HOURS / periodHours;
		}

		using Rings::Band;
		using Rings::Gaussian;

		// The reflectance of the brightest point of Pluto's map, for its mean albedo of 0.55 (see
		// Tools/Textures/make_moon_maps.py)
		constexpr float PLUTO_ALBEDO_SCALE = 1.398f;
	}

	Mercury::Mercury() : Planet(3.3011E23, make<SV::Atmosphere::MercuryAtmosphere>())
	{
		// MESSENGER MDIS global mosaic
		diffuseMap = make<AsyncTexture>("Solar System/Mercury/Mercury 1536x768.jpg");
		rotationSpeed = RotationSpeed(1407.6f);
		litSurface = true;
		// Geometric albedo 0.14
		surfaceAlbedo = 0.22f;
	}

	Venus::Venus() : Planet(4.8675E24, make<SV::Atmosphere::VenusAtmosphere>())
	{
		// The cloud tops (the surface is hidden beneath them)
		diffuseMap = make<AsyncTexture>("Solar System/Venus/Venus Clouds 1536x768.jpg");
		// The clouds' four day super-rotation (retrograde, like the planet's 243 day rotation)
		rotationSpeed = RotationSpeed(-96.0f);
		litSurface = true;
		// Bond albedo 0.76
		surfaceAlbedo = 0.85f;
	}

	Earth::Earth() : Planet(5.9722E24, make<SV::Atmosphere::EarthAtmosphere>())
	{
		// Downscaled from the 3840x1920 NASA maps to keep each decoded texture under 5 MB
		diffuseMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth.jpg");
		specularMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Specular Map.jpg");
		cloudsMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Clouds.jpg");
		nightMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Night.jpg");
		// WGS 84: 6378.137 km equatorial, 6356.752 km polar
		polarRatio = 6356.752f / 6378.137f;
		rotationSpeed = EARTH_ROTATION_SPEED;
	}

	Mars::Mars() : Planet(6.4171E23, make<SV::Atmosphere::MarsAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Mars/Mars 1536x768.jpg");
		// 3396.2 km equatorial, 3376.2 km polar
		polarRatio = 3376.2f / 3396.2f;
		rotationSpeed = RotationSpeed(24.623f);
		litSurface = true;
		// Geometric albedo 0.17 (the bright deserts reach about 0.3)
		surfaceAlbedo = 0.3f;
	}

	Jupiter::Jupiter() : Planet(1.89819E27, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		// Cassini ISS global map
		diffuseMap = make<AsyncTexture>("Solar System/Jupiter/Jupiter Cassini 1536x768.jpg");
		// 1 bar radii (km): 71492 equatorial, 66854 polar
		constexpr double RADIUS_KM = 71492.0;
		polarRatio = 66854.0f / (float)RADIUS_KM;
		rotationSpeed = RotationSpeed(9.925f);
		litSurface = true;
		surfaceAlbedo = 0.6f;

		// Jupiter's rings are all dust, knocked off its inner moons by micrometeoroids, and reddish.
		// Optical depths from Galileo and New Horizons (Burns et al. 2004, Throop et al. 2004).
		//  - The halo, a thick torus inside the main ring
		//  - The main ring, brightest at its outer edge (Adrastea's orbit)
		//  - The Amalthea and Thebe gossamer rings, fed by those moons, each brightest just inside the
		//    moon's orbit
		constexpr double OUTER_KM = 226000.0;
		dustMap = Rings::CreateDustMap(OUTER_KM, [](const double r)
			{
				const double halo = 1.0e-6 * Band(r, 100000.0, 122500.0, 8000.0);
				const double main = 3.0e-6 * Band(r, 122500.0, 128900.0, 300.0) + 3.0e-6 * Band(r, 127000.0, 128900.0, 300.0);
				const double amalthea = 1.0e-7 * Band(r, 129000.0, 181350.0, 1500.0) * (0.3 + 0.7 * std::clamp((r - 129000.0) / 52350.0, 0.0, 1.0));
				const double thebe = 3.0e-8 * Band(r, 129000.0, 221900.0, 3000.0);
				return halo * glm::dvec3(0.9, 0.9, 1.0) + (main + amalthea + thebe) * glm::dvec3(1.0, 0.8, 0.65);
			});
		rings = { 0.0f, 0.0f, (float)(92000.0 / RADIUS_KM), (float)(OUTER_KM / RADIUS_KM) };
	}

	Saturn::Saturn() : Planet(5.6834E26, make<SV::Atmosphere::SaturnAtmosphere>())
	{
		// Cassini ISS global colour map (NASA PDS Atmospheres Node, co_iss_global-maps, 2011-08-11),
		// gaps filled and downscaled to 1536x768
		diffuseMap = make<AsyncTexture>("Solar System/Saturn/Saturn Cassini ISS 1536x768.png");
		// 1 bar radii (km): 60268 equatorial, 54364 polar. Saturn is the most oblate planet (a
		// flattening of 1/10.2), from its fast rotation and low density.
		constexpr double RADIUS_KM = 60268.0;
		polarRatio = 54364.0f / (float)RADIUS_KM;
		// System III rotation period: 10 h 33 min 38 s
		rotationSpeed = RotationSpeed(10.561f);
		litSurface = true;
		surfaceAlbedo = 0.55f;

		// Radial profile of the C, B and A rings and the F ring (the 8K ring map reduced to 8192x4),
		// spanning 71,340 km to 141,000 km: fitted to the C ring's inner edge, the B ring's inner and
		// outer edges, the Cassini Division, the Encke Gap and the A and F rings
		ringMap = make<AsyncTexture>("Solar System/Saturn/Saturn Rings 8192x4.png", Rings::ConvertPhotographicRingMap);

		// The dusty rings, made of micron sized ice grains: almost invisible when lit from the front,
		// they glow when seen against the Sun (Hedman et al.)
		//  - The D ring inside the C ring, with its brighter ringlets D68 and D72
		//  - The F ring's dusty envelope, around its narrow core
		//  - The Janus/Epimetheus ring, along those moons' shared orbit
		//  - The G ring, with a sharp inner edge
		//  - The E ring, fed by the plumes of Enceladus and peaking at its orbit (238,000 km), the
		//    bluest of all rings
		constexpr double OUTER_KM = 480000.0;
		dustMap = Rings::CreateDustMap(OUTER_KM, [](const double r)
			{
				const double dRing = 2.0e-5 * Band(r, 66900.0, 74500.0, 500.0) + 3.0e-4 * Gaussian(r, 67630.0, 100.0, 100.0) +
									 1.0e-4 * Gaussian(r, 71710.0, 150.0, 150.0);
				const double fEnvelope = 0.01 * Gaussian(r, 140220.0, 250.0, 250.0);
				const double janusEpimetheus = 2.0e-6 * Gaussian(r, 151500.0, 1800.0, 1800.0);
				const double gRing = 1.0e-5 * Gaussian(r, 167500.0, 1500.0, 3600.0);
				const double eRing = 1.0e-5 * (r < 238000.0 ? Gaussian(r, 238000.0, 21000.0, 21000.0)
															: std::exp(-(r - 238000.0) / 48000.0));
				// The E ring fades out over its last fifth
				const double eFade = Band(r, 0.0, 0.8 * OUTER_KM, 0.2 * OUTER_KM);
				return (dRing + fEnvelope) * glm::dvec3(0.9, 0.95, 1.0) +
					   (janusEpimetheus + gRing) * glm::dvec3(0.8, 0.9, 1.0) + eRing * eFade * glm::dvec3(0.3, 0.55, 1.0);
			});
		rings = { (float)(71340.0 / RADIUS_KM), (float)(141000.0 / RADIUS_KM), (float)(66900.0 / RADIUS_KM),
				  (float)(OUTER_KM / RADIUS_KM) };
	}

	Uranus::Uranus() : Planet(8.6813E25, make<SV::Atmosphere::UranusAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Uranus/Uranus 1536x768.jpg");
		// 1 bar radii (km): 25559 equatorial, 24973 polar
		constexpr double RADIUS_KM = 25559.0;
		polarRatio = 24973.0f / (float)RADIUS_KM;
		// Retrograde: Uranus is tipped over by 98 degrees
		rotationSpeed = RotationSpeed(-17.24f);
		litSurface = true;
		surfaceAlbedo = 0.6f;

		// Uranus' main rings: narrow, dense and as dark as coal (particle albedo about 0.03), found by
		// stellar occultation in 1977. Radii, widths and optical depths from Voyager 2 and later
		// occultations (French et al. 1991). The epsilon ring is the widest and densest.
		const glm::vec3 DARK(0.03f);
		const Rings::Ringlet ringlets[] = {
			{ 41837.0, 1.6, 0.3, DARK },   // 6
			{ 42234.0, 2.6, 0.5, DARK },   // 5
			{ 42571.0, 2.3, 0.3, DARK },   // 4
			{ 44718.0, 7.0, 0.4, DARK },   // Alpha
			{ 45661.0, 8.0, 0.3, DARK },   // Beta
			{ 47176.0, 1.6, 0.4, DARK },   // Eta
			{ 47627.0, 3.0, 1.5, DARK },   // Gamma
			{ 48300.0, 6.0, 0.5, DARK },   // Delta
			{ 50023.0, 2.0, 0.1, DARK },   // Lambda
			{ 51149.0, 58.0, 1.0, glm::vec3(0.04f) } // Epsilon
		};
		ringMap = Rings::CreateRingMap(41500.0, 51300.0, ringlets);

		// The dust: the zeta ring inside the main rings, dust lanes between them (seen by Voyager
		// looking back towards the Sun), the dusty eta and lambda rings, and the outer nu (red) and mu
		// (blue, fed by the moon Mab like Saturn's E ring) rings found by Hubble in 2003
		constexpr double OUTER_KM = 105000.0;
		dustMap = Rings::CreateDustMap(OUTER_KM, [](const double r)
			{
				const double zeta = 2.0e-5 * Band(r, 37850.0, 41350.0, 800.0);
				const double lanes = 5.0e-6 * Band(r, 41500.0, 51000.0, 300.0) * (0.6 + 0.4 * std::cos(r / 900.0));
				const double eta = 2.0e-3 * Band(r, 47176.0, 47231.0, 10.0);
				const double lambda = 0.1 * Band(r, 50022.0, 50024.0, 1.0);
				const double nu = 4.0e-6 * Gaussian(r, 67300.0, 1300.0, 1300.0);
				const double mu = 8.5e-6 * Gaussian(r, 97700.0, 5000.0, 3000.0);
				return (zeta + lanes + eta + lambda) * glm::dvec3(0.9, 0.9, 0.95) + nu * glm::dvec3(1.0, 0.75, 0.6) +
					   mu * glm::dvec3(0.3, 0.55, 1.0);
			});
		rings = { (float)(41500.0 / RADIUS_KM), (float)(51300.0 / RADIUS_KM), (float)(37000.0 / RADIUS_KM),
				  (float)(OUTER_KM / RADIUS_KM) };
	}

	Neptune::Neptune() : Planet(1.02409E26, make<SV::Atmosphere::NeptuneAtmosphere>())
	{
		// Voyager 2 map
		diffuseMap = make<AsyncTexture>("Solar System/Neptune/Neptune 1024x512.jpg");
		// 1 bar radii (km): 24764 equatorial, 24341 polar
		constexpr double RADIUS_KM = 24764.0;
		polarRatio = 24341.0f / (float)RADIUS_KM;
		rotationSpeed = RotationSpeed(16.11f);
		litSurface = true;
		surfaceAlbedo = 0.55f;

		// Neptune's rings are faint, dusty and red (Voyager 2; de Pater et al. 2018): the narrow Le
		// Verrier and Adams rings, and the broad Galle, Lassell and Arago rings. The Adams ring's arcs
		// (clumps along 40 degrees of its length) are left out, as the rings are drawn axisymmetric.
		const glm::vec3 RED(0.05f, 0.042f, 0.036f);
		const Rings::Ringlet ringlets[] = {
			{ 53200.0, 113.0, 0.004, RED }, // Le Verrier
			{ 62932.0, 35.0, 0.004, RED }   // Adams
		};
		ringMap = Rings::CreateRingMap(52000.0, 64000.0, ringlets);

		constexpr double OUTER_KM = 64000.0;
		dustMap = Rings::CreateDustMap(OUTER_KM, [](const double r)
			{
				const double galle = 1.0e-4 * Band(r, 40900.0, 42900.0, 300.0);
				const double leVerrier = 4.0e-3 * Band(r, 53150.0, 53250.0, 20.0);
				const double lassell = 1.5e-4 * Band(r, 53200.0, 57200.0, 300.0);
				const double arago = 5.0e-4 * Band(r, 57150.0, 57250.0, 20.0);
				const double adams = 3.0e-3 * Band(r, 62915.0, 62950.0, 5.0);
				return (galle + leVerrier + lassell + arago + adams) * glm::dvec3(1.0, 0.85, 0.75);
			});
		rings = { (float)(52000.0 / RADIUS_KM), (float)(64000.0 / RADIUS_KM), (float)(40000.0 / RADIUS_KM),
				  (float)(OUTER_KM / RADIUS_KM) };
	}

	Pluto::Pluto() : Planet(1.3030E22, make<SV::Atmosphere::PlutoAtmosphere>())
	{
		// The New Horizons LORRI and MVIC global mosaic (USGS Astrogeology), coloured by its
		// brightness from the reddish tholins of the dark areas to the pale nitrogen ice of Sputnik
		// Planitia (see Tools/Textures/make_moon_maps.py). The south was in the dark of its long
		// winter during the flyby and is filled in.
		diffuseMap = make<AsyncTexture>("Solar System/Pluto/Pluto 1536x768.jpg");
		// Turned by FaceMoon instead
		rotationSpeed = 0.0f;
		litSurface = true;
		surfaceAlbedo = PLUTO_ALBEDO_SCALE;
	}

	void Pluto::FaceMoon(const glm::dvec3& offset, const glm::dvec3& orbitPole)
	{
		glm::dvec3 forward = offset - orbitPole * glm::dot(offset, orbitPole);
		if (glm::dot(forward, forward) < 1e-30)
			return;
		forward = glm::normalize(forward);
		const glm::dvec3 east = glm::cross(orbitPole, forward);
		SetOrientation(glm::mat3(glm::vec3(east), glm::vec3(orbitPole), glm::vec3(forward)));
	}

#pragma endregion

#pragma region Planet New Renderer

	namespace
	{
		// Mirrors Shaders/Include/PlanetPushConstants.glsl
		struct PlanetPushConstants
		{
			glm::mat4 transform;
			glm::vec4 axes[3];
			glm::vec4 shape;
		};
		static_assert(sizeof(PlanetPushConstants) == 128);

		// Mirrors Shaders/Include/RingsPushConstants.glsl
		struct RingPushConstants
		{
			glm::mat4 transform;
			glm::vec4 axes[3];
			glm::vec4 rings;
		};
		static_assert(sizeof(RingPushConstants) == 128);

		constexpr int PLANET_HAS_SPECULAR = 1;
		constexpr int PLANET_HAS_CLOUDS = 2;
		constexpr int PLANET_HAS_RINGS = 4;
		constexpr int PLANET_HAS_NIGHT = 8;
		constexpr int PLANET_LIT_SURFACE = 16;

		constexpr unsigned int SPHERE_SUBDIVISIONS = 50;

		// The smallest disc drawn, in pixels of radius, and how far a smaller body's disc may be
		// inflated to it (beyond that, at under 1/400 of its drawn brightness, it is skipped)
		constexpr float MIN_DISC_PIXELS = 0.75f;
		constexpr float MAX_DISC_INFLATION = 20.0f;
		constexpr unsigned int RINGS_SEGMENTS = 120;

		// The half ring mesh lies in the XY plane with y >= 0; this turns it into the XZ plane (z >= 0)
		const glm::mat4 RING_ROTATION_CORRECTION = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		DH::BindGroupLayoutEntry FragmentTexture(uint32_t binding, bool is3D = false)
		{
			return { binding,
					 is3D ? DH::ShaderResourceType::CombinedTextureSampler3D : DH::ShaderResourceType::CombinedTextureSampler,
					 DH::ShaderStage::Fragment };
		}

		// The planet's rotation (planet space to world), without its scale
		glm::mat3 GetPlanetRotation(const Planet& planet)
		{
			return glm::mat3(planet.GetRotationMatrix());
		}

		// World units per equatorial radius
		float GetUnitsPerRadius(const Planet& planet)
		{
			return planet.GetScale().x;
		}
	}

	PlanetRenderer::PlanetRenderer(const SceneContext& contextIn) : context(contextIn)
	{
		atmosphereLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::Fragment },
			FragmentTexture(1),
			FragmentTexture(2, true) });
		mapsLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0), FragmentTexture(1), FragmentTexture(2), FragmentTexture(3), FragmentTexture(4) });
		ringLayout = DH::BindGroupLayout::Create({ FragmentTexture(0), FragmentTexture(1) });
		ringShadowLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::Fragment },
			FragmentTexture(1) });

		// Every planet mesh has the same vertex layout
		const sRef<DH::GPUMesh>& sampleSkyMesh = GetSkyMesh(1.0f);
		DH::GraphicsPipelineDesc surfaceDesc;
		surfaceDesc.vertexShader = Shaders::Create("Planet.vert");
		surfaceDesc.fragmentShader = Shaders::Create("Planet.frag");
		surfaceDesc.vertexBindings = sampleSkyMesh->GetVertexBindings();
		surfaceDesc.vertexAttributes = sampleSkyMesh->GetVertexAttributes();
		surfaceDesc.raster.cullMode = DH::CullMode::Back;
		surfaceDesc.depth = { true, true, DH::CompareOp::Less };
		// Premultiplied: the atmosphere's translucent limb adds its light and hides what is behind it
		// by its opacity
		surfaceDesc.blend = DH::BlendState(DH::BlendFactor::One, DH::BlendFactor::OneMinusSrcAlpha);
		surfaceDesc.targetLayout = context.targetLayout;
		surfaceDesc.bindGroupLayouts = { context.frameLayout, atmosphereLayout, mapsLayout };
		surfaceDesc.pushConstantSize = sizeof(PlanetPushConstants);
		surfacePipeline = ModalPipeline(surfaceDesc);

		const sRef<DH::GPUMesh> sampleRingsMesh = DH::GPUMesh::Create(DH::Geometry::CreateHalfRings(1.0f, 2.0f, 4));
		DH::GraphicsPipelineDesc ringsDesc;
		ringsDesc.vertexShader = Shaders::Create("Rings.vert");
		ringsDesc.fragmentShader = Shaders::Create("Rings.frag");
		ringsDesc.vertexBindings = sampleRingsMesh->GetVertexBindings();
		ringsDesc.vertexAttributes = sampleRingsMesh->GetVertexAttributes();
		// Both faces are drawn: the shader works out which face (lit or unlit) is seen
		ringsDesc.raster.cullMode = DH::CullMode::None;
		ringsDesc.depth = { true, true, DH::CompareOp::Less };
		// Premultiplied: the rings add the light they scatter and hide what is behind them by their opacity
		ringsDesc.blend = DH::BlendState(DH::BlendFactor::One, DH::BlendFactor::OneMinusSrcAlpha);
		ringsDesc.targetLayout = context.targetLayout;
		ringsDesc.bindGroupLayouts = { context.frameLayout, ringLayout, ringShadowLayout };
		ringsDesc.pushConstantSize = sizeof(RingPushConstants);
		ringsPipeline = ModalPipeline(ringsDesc);
	}

	PlanetRenderer::~PlanetRenderer()
	{
		for (const auto& precomputer : finishedPrecomputers)
			precomputer->ReleasePrecomputeResources();
	}

	const sRef<DH::GPUMesh>& PlanetRenderer::GetSkyMesh(const float polarRatio)
	{
		sRef<DH::GPUMesh>& mesh = skyMeshes[polarRatio];
		if (mesh == nullptr)
			mesh = DH::GPUMesh::Create(DH::Geometry::CreateEllipsoid(glm::vec3(1.0f, polarRatio, 1.0f), SPHERE_SUBDIVISIONS));
		return mesh;
	}

	void PlanetRenderer::UpdateMapBindings(const Planet& planet, PlanetBindings& bindings)
	{
		bindings.mapFlags = planet.HasLitSurface() ? PLANET_LIT_SURFACE : 0;
		auto TextureOrBlack = [this, &bindings](const sRef<AsyncTexture>& map, int flag) -> sRef<DH::Texture>
		{
			if (map == nullptr || !map->IsReady())
				return context.blackTexture;
			bindings.mapFlags |= flag;
			return map->GetTexture();
		};

		using Entry = DH::BindGroupEntry;
		const sRef<DH::Texture> diffuseMap = TextureOrBlack(planet.GetDiffuseMap(), 0);
		const sRef<DH::Texture> specularMap = TextureOrBlack(planet.GetSpecularMap(), PLANET_HAS_SPECULAR);
		const sRef<DH::Texture> cloudsMap = TextureOrBlack(planet.GetCloudsMap(), PLANET_HAS_CLOUDS);
		const sRef<DH::Texture> ringMap = TextureOrBlack(planet.GetRingMap(), PLANET_HAS_RINGS);
		const sRef<DH::Texture> nightMap = TextureOrBlack(planet.GetNightMap(), PLANET_HAS_NIGHT);

		// Shared with any other body with the same maps
		const std::array<const void*, 6> key = { diffuseMap.get(), specularMap.get(), cloudsMap.get(), ringMap.get(), nightMap.get(),
												 reinterpret_cast<const void*>((uintptr_t)bindings.mapFlags) };
		sRef<DH::BindGroup>& mapsGroup = mapsGroups[key];
		if (mapsGroup == nullptr)
		{
			mapsGroup = DH::BindGroup::Create(mapsLayout, {
				Entry::TextureSampler(0, diffuseMap, context.mapSampler),
				Entry::TextureSampler(1, specularMap, context.mapSampler),
				Entry::TextureSampler(2, cloudsMap, context.mapSampler),
				Entry::TextureSampler(3, ringMap, context.ringSampler),
				Entry::TextureSampler(4, nightMap, context.mapSampler) });
		}
		bindings.mapsGroup = mapsGroup;
		// Drop groups no body uses any more (such as those for maps before they loaded)
		for (auto it = mapsGroups.begin(); it != mapsGroups.end();)
			it = it->second.use_count() == 1 && it->second != bindings.mapsGroup ? mapsGroups.erase(it) : std::next(it);

		// The rings are drawn once either map is ready, over the extent of the maps that are
		bindings.hasRingMap = (bindings.mapFlags & PLANET_HAS_RINGS) != 0;
		bindings.hasDustMap = planet.GetDustMap() != nullptr && planet.GetDustMap()->IsReady();
		if (bindings.hasRingMap || bindings.hasDustMap)
		{
			const PlanetRings& rings = planet.GetRings();
			float inner = std::numeric_limits<float>::max(), outer = 0.0f;
			if (bindings.hasRingMap)
			{
				inner = std::min(inner, rings.innerRadius);
				outer = std::max(outer, rings.outerRadius);
			}
			if (bindings.hasDustMap)
			{
				inner = std::min(inner, rings.dustInnerRadius);
				outer = std::max(outer, rings.dustOuterRadius);
			}
			bindings.ringGroup = DH::BindGroup::Create(ringLayout, {
				Entry::TextureSampler(0, ringMap, context.ringSampler),
				Entry::TextureSampler(1, bindings.hasDustMap ? planet.GetDustMap()->GetTexture() : context.blackTexture,
									  context.ringSampler) });
			bindings.ringsMesh = DH::GPUMesh::Create(DH::Geometry::CreateHalfRings(inner, outer, RINGS_SEGMENTS));
		}
		else
		{
			bindings.ringGroup = nullptr;
			bindings.ringsMesh = nullptr;
		}
	}

	void PlanetRenderer::Prepare(DH::CommandList& cmd)
	{
		// Precomputations recorded in an earlier frame have been submitted, so their temporary
		// resources can go once the GPU is idle
		for (const auto& precomputer : finishedPrecomputers)
			precomputer->ReleasePrecomputeResources();
		finishedPrecomputers.clear();

		for (const sRef<Planet>& planet : planets)
		{
			const sRef<SV::Atmosphere::IAtmosphere>& atmosphere = planet->GetAtmosphere();
			if (atmosphere == nullptr)
				continue;

			auto [it, isNew] = planetBindings.try_emplace(planet.get());
			PlanetBindings& bindings = it->second;

			const sRef<SV::Atmosphere::AtmospherePrecomputer>& precomputer = atmosphere->GetPrecomputer();
			if (!precomputer->IsPrecomputed())
			{
				precomputer->PrecomputeTextures(cmd, context);
				finishedPrecomputers.push_back(precomputer);
			}

			if (isNew)
			{
				// Shared by the bodies with this atmosphere (all of a planet's airless moons)
				using Entry = DH::BindGroupEntry;
				AtmosphereGroups& groups = atmosphereGroups[precomputer.get()];
				if (groups.atmosphere == nullptr)
				{
					groups.atmosphere = DH::BindGroup::Create(atmosphereLayout, {
						Entry::UniformBuffer(0, precomputer->GetUniformBuffer()),
						Entry::TextureSampler(1, precomputer->GetTransmittanceTex(), context.clampSampler),
						Entry::TextureSampler(2, precomputer->GetScatteringTex(), context.clampSampler) });
				}
				// Only planets with rings need their atmosphere to shade their rings
				if (groups.ringShadow == nullptr && (planet->GetRingMap() != nullptr || planet->GetDustMap() != nullptr))
				{
					groups.ringShadow = DH::BindGroup::Create(ringShadowLayout, {
						Entry::UniformBuffer(0, precomputer->GetUniformBuffer()),
						Entry::TextureSampler(1, precomputer->GetTransmittanceTex(), context.clampSampler) });
				}
				bindings.atmosphereGroup = groups.atmosphere;
				bindings.ringShadowGroup = groups.ringShadow;
				bindings.skyMesh = GetSkyMesh(planet->GetPolarRatio());
			}

			// Poll every map (each upload happens on the call that sees it finish loading)
			bool mapsChanged = isNew;
			for (const sRef<AsyncTexture>* map : { &planet->GetDiffuseMap(), &planet->GetSpecularMap(), &planet->GetCloudsMap(),
												   &planet->GetNightMap(), &planet->GetRingMap(), &planet->GetDustMap() })
			{
				if (*map != nullptr && (*map)->Poll())
					mapsChanged = true;
			}
			if (mapsChanged)
				UpdateMapBindings(*planet, bindings);
		}
	}

	PlanetRenderer::MapLoadProgress PlanetRenderer::GetMapLoadProgress() const
	{
		MapLoadProgress progress;
		for (const sRef<Planet>& planet : planets)
		{
			for (const sRef<AsyncTexture>* map : { &planet->GetDiffuseMap(), &planet->GetSpecularMap(), &planet->GetCloudsMap(),
												   &planet->GetNightMap(), &planet->GetRingMap(), &planet->GetDustMap() })
			{
				if (*map == nullptr)
					continue;
				progress.total++;
				if (!(*map)->IsPending())
					progress.loaded++;
			}
		}
		return progress;
	}

	void PlanetRenderer::RenderRings(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const glm::vec3& cameraPosition, const bool isBack)
	{
		bool pipelineBound = false;
		for (const auto& [distance, index] : distanceCache)
		{
			const Planet& planet = *planets[index];
			const auto it = planetBindings.find(&planet);
			if (it == planetBindings.end() || it->second.ringGroup == nullptr)
				continue;
			const PlanetBindings& bindings = it->second;

			if (!pipelineBound)
			{
				cmd.BindPipeline(ringsPipeline.Get(polygonMode));
				pipelineBound = true;
			}

			// Turn the half ring towards the camera (in the ring plane) for the front half, or away
			// from it for the back half
			const glm::mat3 rotation = GetPlanetRotation(planet);
			const glm::vec3 cameraInPlanet = glm::transpose(rotation) * (cameraPosition - planet.GetTranslation());
			const float towardsCamera = std::atan2(cameraInPlanet.x, cameraInPlanet.z);
			const float yaw = isBack ? towardsCamera + glm::pi<float>() : towardsCamera;
			const glm::mat4 halfRotation = glm::rotate(yaw, glm::vec3(0.0f, 1.0f, 0.0f));

			const PlanetRings& rings = planet.GetRings();
			const RingPushConstants pushConstants{
				planet.transformationMatrix * halfRotation * RING_ROTATION_CORRECTION,
				{ glm::vec4(rotation[0], GetUnitsPerRadius(planet)),
				  glm::vec4(rotation[1], planet.GetPolarRatio()),
				  glm::vec4(rotation[2], 0.0f) },
				// A ring map not yet loaded is skipped by giving it no extent, a dust map by an outer radius of 0
				glm::vec4(bindings.hasRingMap ? rings.innerRadius : 0.0f, bindings.hasRingMap ? rings.outerRadius : 0.0f,
						  bindings.hasDustMap ? rings.dustOuterRadius : 0.0f, diffuseRingGain)
			};
			cmd.BindGroup(1, *bindings.ringGroup);
			cmd.BindGroup(2, *bindings.ringShadowGroup);
			cmd.PushConstants(pushConstants);
			bindings.ringsMesh->Draw(cmd);
		}
	}

	void PlanetRenderer::Render(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const glm::vec3& cameraPosition,
								const float pixelAngle)
	{
		if (planets.empty()) return;

		for (const sRef<Planet>& planet : planets)
			planet->RecalculateTransformation();

		RenderRings(cmd, polygonMode, cameraPosition, true);

		cmd.BindPipeline(surfacePipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const Planet& planet = *planets[index];
			const auto it = planetBindings.find(&planet);
			if (it == planetBindings.end())
				continue;
			const PlanetBindings& bindings = it->second;
			const SV::Atmosphere::ProcessedAtmoData& atmoData = planet.GetAtmosphere()->GetAtmosphericData();
			const float skyScale = atmoData.topRadius / atmoData.bottomRadius;

			// Inflated to the smallest disc drawn (the shader shrinks it back, and dims it by the
			// inflation squared), or skipped if that would leave it too faint to see
			const float angularRadius = GetUnitsPerRadius(planet) * skyScale / glm::distance(planet.GetTranslation(), cameraPosition);
			const float inflation = std::max(MIN_DISC_PIXELS * pixelAngle / angularRadius, 1.0f);
			if (inflation > MAX_DISC_INFLATION)
				continue;

			// The mesh is drawn at the top of the atmosphere; the surface is found in the shader
			const glm::mat4 skyTransformation = planet.transformationMatrix * glm::scale(glm::vec3(skyScale * inflation));
			const glm::mat3 rotation = GetPlanetRotation(planet);
			const PlanetRings& rings = planet.GetRings();

			const PlanetPushConstants pushConstants{
				skyTransformation,
				{ glm::vec4(rotation[0], planet.HasLitSurface() ? planet.GetSurfaceAlbedo()
															  : planet.GetAtmosphere()->GetSurfaceAtmoIntensity()),
				  glm::vec4(rotation[1], (float)bindings.mapFlags),
				  glm::vec4(rotation[2], GetUnitsPerRadius(planet)) },
				glm::vec4(planet.GetPolarRatio(), rings.innerRadius, rings.outerRadius, planet.GetRotationSpeed())
			};

			cmd.BindGroup(1, *bindings.atmosphereGroup);
			cmd.BindGroup(2, *bindings.mapsGroup);
			cmd.PushConstants(pushConstants);
			bindings.skyMesh->Draw(cmd);
		}

		RenderRings(cmd, polygonMode, cameraPosition, false);
	}

#pragma endregion
}
