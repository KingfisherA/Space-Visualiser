#include "PlanetNew.hpp"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Maths/Geometry.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"

namespace SV
{
#pragma region Planets

	namespace
	{
		// Earth's surface map turns this much per unit of planet animation time
		constexpr float EARTH_ROTATION_SPEED = 0.3f;
		constexpr float EARTH_DAY_HOURS = 24.0f;
	}

	Mercury::Mercury() : PlanetNew(3.285E23, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Mercury/Mercury Diffuse.png");
	}

	Venus::Venus() : PlanetNew(4.867E24, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Venus/Venus Surface 1080p.jpg");
		cloudsMap = make<AsyncTexture>("Solar System/Venus/Venus Clouds 1080p.png");
	}

	Earth::Earth() : PlanetNew(5.972E24, make<SV::Atmosphere::EarthAtmosphere>())
	{
		// Downscaled from the 3840x1920 NASA maps to keep each decoded texture under 5 MB
		diffuseMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth.jpg");
		specularMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Specular Map.jpg");
		cloudsMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Clouds.jpg");
		nightMap = make<AsyncTexture>("Solar System/Earth/NASA/1536x768/Earth Night.jpg");
		rotationSpeed = EARTH_ROTATION_SPEED;
	}

	Mars::Mars() : PlanetNew(6.39E23, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Mars/Mars Surface.png");
	}

	Jupiter::Jupiter() : PlanetNew(1.898E27, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Jupiter/Jupiter Cassini Med Resolution Map.jpg");
		ringMap = make<AsyncTexture>("Solar System/Jupiter/Jupiter Rings.png");
		rings = { 1.5f, 3.2f };
		// 1 bar radii (km): 71492 equatorial, 66854 polar
		polarRatio = 66854.0f / 71492.0f;
		rotationSpeed = EARTH_ROTATION_SPEED * EARTH_DAY_HOURS / 9.925f;
		litSurface = true;
	}

	Saturn::Saturn() : PlanetNew(5.683E26, make<SV::Atmosphere::SaturnAtmosphere>())
	{
		// Cassini ISS global colour map (NASA PDS Atmospheres Node, co_iss_global-maps, 2011-08-11),
		// gaps filled and downscaled to 1536x768
		diffuseMap = make<AsyncTexture>("Solar System/Saturn/Saturn Cassini ISS 1536x768.png");
		// Radial profile of the D to F rings (the 8K ring map reduced to 8192x4)
		ringMap = make<AsyncTexture>("Solar System/Saturn/Saturn Rings 8192x4.png");

		// The ring map spans 71,340 km to 141,000 km: fitted to the C ring's inner edge, the B ring's
		// inner and outer edges, the Cassini Division, the Encke Gap and the A and F rings
		constexpr float EQUATORIAL_RADIUS_KM = 60268.0f;
		rings = { 71340.0f / EQUATORIAL_RADIUS_KM, 141000.0f / EQUATORIAL_RADIUS_KM };
		// 1 bar radii (km): 60268 equatorial, 54364 polar. Saturn is the most oblate planet (a
		// flattening of 1/10.2), from its fast rotation and low density.
		polarRatio = 54364.0f / EQUATORIAL_RADIUS_KM;
		// System III rotation period: 10 h 33 min 38 s
		rotationSpeed = EARTH_ROTATION_SPEED * EARTH_DAY_HOURS / 10.561f;
		litSurface = true;
	}

	// Titan has no surface map: its thick atmosphere is drawn over a black surface
	Titan::Titan() : PlanetNew(1.3452E23, make<SV::Atmosphere::TitanAtmosphere>())
	{
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
		glm::mat3 GetPlanetRotation(const PlanetNew& planet)
		{
			return glm::mat3(planet.GetRotationMatrix());
		}

		// World units per equatorial radius
		float GetUnitsPerRadius(const PlanetNew& planet)
		{
			return planet.GetScale().x;
		}
	}

	PlanetNewRenderer::PlanetNewRenderer(const SceneContext& contextIn) : context(contextIn)
	{
		atmosphereLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::Fragment },
			FragmentTexture(1),
			FragmentTexture(2, true) });
		mapsLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0), FragmentTexture(1), FragmentTexture(2), FragmentTexture(3), FragmentTexture(4) });
		ringLayout = DH::BindGroupLayout::Create({ FragmentTexture(0) });
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
		// The atmosphere's limb is translucent
		surfaceDesc.blend = DH::BlendPreset::Alpha;
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

	PlanetNewRenderer::~PlanetNewRenderer()
	{
		for (const auto& precomputer : finishedPrecomputers)
			precomputer->ReleasePrecomputeResources();
	}

	const sRef<DH::GPUMesh>& PlanetNewRenderer::GetSkyMesh(const float polarRatio)
	{
		sRef<DH::GPUMesh>& mesh = skyMeshes[polarRatio];
		if (mesh == nullptr)
			mesh = DH::GPUMesh::Create(DH::Geometry::CreateEllipsoid(glm::vec3(1.0f, polarRatio, 1.0f), SPHERE_SUBDIVISIONS));
		return mesh;
	}

	void PlanetNewRenderer::UpdateMapBindings(const PlanetNew& planet, PlanetBindings& bindings)
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
		const sRef<DH::Texture> ringMap = TextureOrBlack(planet.GetRingMap(), PLANET_HAS_RINGS);
		bindings.mapsGroup = DH::BindGroup::Create(mapsLayout, {
			Entry::TextureSampler(0, TextureOrBlack(planet.GetDiffuseMap(), 0), context.mapSampler),
			Entry::TextureSampler(1, TextureOrBlack(planet.GetSpecularMap(), PLANET_HAS_SPECULAR), context.mapSampler),
			Entry::TextureSampler(2, TextureOrBlack(planet.GetCloudsMap(), PLANET_HAS_CLOUDS), context.mapSampler),
			Entry::TextureSampler(3, ringMap, context.ringSampler),
			Entry::TextureSampler(4, TextureOrBlack(planet.GetNightMap(), PLANET_HAS_NIGHT), context.mapSampler) });

		if ((bindings.mapFlags & PLANET_HAS_RINGS) != 0)
		{
			const PlanetRings& rings = planet.GetRings();
			bindings.ringGroup = DH::BindGroup::Create(ringLayout, { Entry::TextureSampler(0, ringMap, context.ringSampler) });
			bindings.ringsMesh = DH::GPUMesh::Create(DH::Geometry::CreateHalfRings(rings.innerRadius, rings.outerRadius, RINGS_SEGMENTS));
		}
		else
		{
			bindings.ringGroup = nullptr;
			bindings.ringsMesh = nullptr;
		}
	}

	void PlanetNewRenderer::Prepare(DH::CommandList& cmd)
	{
		// Precomputations recorded in an earlier frame have been submitted, so their temporary
		// resources can go once the GPU is idle
		for (const auto& precomputer : finishedPrecomputers)
			precomputer->ReleasePrecomputeResources();
		finishedPrecomputers.clear();

		for (const sRef<PlanetNew>& planet : planets)
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
				using Entry = DH::BindGroupEntry;
				bindings.atmosphereGroup = DH::BindGroup::Create(atmosphereLayout, {
					Entry::UniformBuffer(0, precomputer->GetUniformBuffer()),
					Entry::TextureSampler(1, precomputer->GetTransmittanceTex(), context.clampSampler),
					Entry::TextureSampler(2, precomputer->GetScatteringTex(), context.clampSampler) });
				bindings.ringShadowGroup = DH::BindGroup::Create(ringShadowLayout, {
					Entry::UniformBuffer(0, precomputer->GetUniformBuffer()),
					Entry::TextureSampler(1, precomputer->GetTransmittanceTex(), context.clampSampler) });
				bindings.skyMesh = GetSkyMesh(planet->GetPolarRatio());
			}

			// Poll every map (each upload happens on the call that sees it finish loading)
			bool mapsChanged = isNew;
			for (const sRef<AsyncTexture>* map : { &planet->GetDiffuseMap(), &planet->GetSpecularMap(), &planet->GetCloudsMap(),
												   &planet->GetNightMap(), &planet->GetRingMap() })
			{
				if (*map != nullptr && (*map)->Poll())
					mapsChanged = true;
			}
			if (mapsChanged)
				UpdateMapBindings(*planet, bindings);
		}
	}

	void PlanetNewRenderer::RenderRings(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const glm::vec3& cameraPosition, const bool isBack)
	{
		bool pipelineBound = false;
		for (const auto& [distance, index] : distanceCache)
		{
			const PlanetNew& planet = *planets[index];
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
				glm::vec4(rings.innerRadius, rings.outerRadius, 0.0f, 0.0f)
			};
			cmd.BindGroup(1, *bindings.ringGroup);
			cmd.BindGroup(2, *bindings.ringShadowGroup);
			cmd.PushConstants(pushConstants);
			bindings.ringsMesh->Draw(cmd);
		}
	}

	void PlanetNewRenderer::Render(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const glm::vec3& cameraPosition)
	{
		if (planets.empty()) return;

		for (const sRef<PlanetNew>& planet : planets)
			planet->RecalculateTransformation();

		RenderRings(cmd, polygonMode, cameraPosition, true);

		cmd.BindPipeline(surfacePipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const PlanetNew& planet = *planets[index];
			const auto it = planetBindings.find(&planet);
			if (it == planetBindings.end())
				continue;
			const PlanetBindings& bindings = it->second;
			const SV::Atmosphere::ProcessedAtmoData& atmoData = planet.GetAtmosphere()->GetAtmosphericData();

			// The mesh is drawn at the top of the atmosphere; the surface is found in the shader
			const glm::mat4 skyTransformation = planet.transformationMatrix * glm::scale(glm::vec3(atmoData.topRadius / atmoData.bottomRadius));
			const glm::mat3 rotation = GetPlanetRotation(planet);
			const PlanetRings& rings = planet.GetRings();

			const PlanetPushConstants pushConstants{
				skyTransformation,
				{ glm::vec4(rotation[0], planet.GetAtmosphere()->GetSurfaceAtmoIntensity()),
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
