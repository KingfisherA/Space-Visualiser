#include "PlanetNew.hpp"

#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Maths/Geometry.h"
#include "Dahlia/Maths/Maths.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"

namespace SV
{
#pragma region Planets

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
		diffuseMap = make<AsyncTexture>("Solar System/Earth/NASA/Earth.jpg");
		specularMap = make<AsyncTexture>("Solar System/Earth/NASA/Earth Specular Map.jpg");
		cloudsMap = make<AsyncTexture>("Solar System/Earth/NASA/Earth Clouds.jpg");
		nightMap = make<AsyncTexture>("Solar System/Earth/NASA/Earth Night.jpg");
	}

	Mars::Mars() : PlanetNew(6.39E23, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Mars/Mars Surface.png");
	}

	Jupiter::Jupiter() : PlanetNew(1.898E27, make<SV::Atmosphere::JupiterAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Jupiter/Jupiter Cassini Med Resolution Map.jpg");
		ringShadowMap = make<AsyncTexture>("Solar System/Jupiter/Jupiter Rings.png");
	}

	Saturn::Saturn() : PlanetNew(5.683E26, make<SV::Atmosphere::SaturnAtmosphere>())
	{
		diffuseMap = make<AsyncTexture>("Solar System/Saturn/8K Saturn Map.jpg");
		ringShadowMap = make<AsyncTexture>("Solar System/Saturn/8K Saturn Ring Map.png");
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
			glm::vec4 normalMatrix[3];
		};

		// Mirrors Shaders/Include/RingsPushConstants.glsl
		struct RingPushConstants
		{
			glm::mat4 transform;
			glm::vec4 properties; // ring width, planet radius, inverse shadow softness
		};

		constexpr int PLANET_HAS_SPECULAR = 1;
		constexpr int PLANET_HAS_CLOUDS = 2;
		constexpr int PLANET_HAS_RING_SHADOW = 4;
		constexpr int PLANET_HAS_NIGHT = 8;

		constexpr unsigned int SPHERE_SUBDIVISIONS = 50;

		constexpr float RINGS_INNER_RAD = 1.5f;
		constexpr float RINGS_OUTER_RAD = 3.2f;
		constexpr unsigned int RINGS_SEGMENTS = 60;
		constexpr float RINGS_PLANET_RADIUS = 0.95f;
		constexpr float RINGS_INV_SHADOW_SOFTNESS = 10.0f;

		const glm::mat4 RING_ROTATION_CORRECTION = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		DH::BindGroupLayoutEntry FragmentTexture(uint32_t binding, bool is3D = false)
		{
			return { binding,
					 is3D ? DH::ShaderResourceType::CombinedTextureSampler3D : DH::ShaderResourceType::CombinedTextureSampler,
					 DH::ShaderStage::Fragment };
		}
	}

	PlanetNewRenderer::PlanetNewRenderer(const SceneContext& contextIn) : context(contextIn)
	{
		planetMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuadsphere(1.0f, SPHERE_SUBDIVISIONS));
		ringsMesh = DH::GPUMesh::Create(DH::Geometry::CreateHalfRings(RINGS_INNER_RAD, RINGS_OUTER_RAD, RINGS_SEGMENTS));

		atmosphereLayout = DH::BindGroupLayout::Create({
			{ 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::Fragment },
			FragmentTexture(1),
			FragmentTexture(2, true) });
		mapsLayout = DH::BindGroupLayout::Create({
			FragmentTexture(0), FragmentTexture(1), FragmentTexture(2), FragmentTexture(3), FragmentTexture(4) });
		ringLayout = DH::BindGroupLayout::Create({ FragmentTexture(0) });

		DH::GraphicsPipelineDesc surfaceDesc;
		surfaceDesc.vertexShader = Shaders::Create("Planet.vert");
		surfaceDesc.fragmentShader = Shaders::Create("Planet.frag");
		surfaceDesc.vertexBindings = planetMesh->GetVertexBindings();
		surfaceDesc.vertexAttributes = planetMesh->GetVertexAttributes();
		surfaceDesc.raster.cullMode = DH::CullMode::Back;
		surfaceDesc.depth = { true, true, DH::CompareOp::Less };
		// The atmosphere's limb is translucent
		surfaceDesc.blend = DH::BlendPreset::Alpha;
		surfaceDesc.targetLayout = context.targetLayout;
		surfaceDesc.bindGroupLayouts = { context.frameLayout, atmosphereLayout, mapsLayout };
		surfaceDesc.pushConstantSize = sizeof(PlanetPushConstants);
		surfacePipeline = ModalPipeline(surfaceDesc);

		DH::GraphicsPipelineDesc ringsDesc;
		ringsDesc.vertexShader = Shaders::Create("Rings.vert");
		ringsDesc.fragmentShader = Shaders::Create("Rings.frag");
		ringsDesc.vertexBindings = ringsMesh->GetVertexBindings();
		ringsDesc.vertexAttributes = ringsMesh->GetVertexAttributes();
		// Both faces are lit (the shader picks the normal from gl_FrontFacing)
		ringsDesc.raster.cullMode = DH::CullMode::None;
		ringsDesc.depth = { true, true, DH::CompareOp::Less };
		ringsDesc.blend = DH::BlendPreset::Alpha;
		ringsDesc.targetLayout = context.targetLayout;
		ringsDesc.bindGroupLayouts = { context.frameLayout, ringLayout };
		ringsDesc.pushConstantSize = sizeof(RingPushConstants);
		ringsPipeline = ModalPipeline(ringsDesc);
	}

	PlanetNewRenderer::~PlanetNewRenderer()
	{
		for (const auto& precomputer : finishedPrecomputers)
			precomputer->ReleasePrecomputeResources();
	}

	void PlanetNewRenderer::UpdateMapBindings(const PlanetNew& planet, PlanetBindings& bindings)
	{
		bindings.mapFlags = 0;
		auto TextureOrBlack = [this, &bindings](const sRef<AsyncTexture>& map, int flag) -> sRef<DH::Texture>
		{
			if (map == nullptr || !map->IsReady())
				return context.blackTexture;
			bindings.mapFlags |= flag;
			return map->GetTexture();
		};

		using Entry = DH::BindGroupEntry;
		const sRef<DH::Texture> ringMap = TextureOrBlack(planet.GetRingsShadowMap(), PLANET_HAS_RING_SHADOW);
		bindings.mapsGroup = DH::BindGroup::Create(mapsLayout, {
			Entry::TextureSampler(0, TextureOrBlack(planet.GetDiffuseMap(), 0), context.mapSampler),
			Entry::TextureSampler(1, TextureOrBlack(planet.GetSpecularMap(), PLANET_HAS_SPECULAR), context.mapSampler),
			Entry::TextureSampler(2, TextureOrBlack(planet.GetCloudsMap(), PLANET_HAS_CLOUDS), context.mapSampler),
			Entry::TextureSampler(3, ringMap, context.ringSampler),
			Entry::TextureSampler(4, TextureOrBlack(planet.GetNightMap(), PLANET_HAS_NIGHT), context.mapSampler) });

		bindings.ringGroup = (bindings.mapFlags & PLANET_HAS_RING_SHADOW) != 0
			? DH::BindGroup::Create(ringLayout, { Entry::TextureSampler(0, ringMap, context.ringSampler) })
			: nullptr;
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
				bindings.atmosphereGroup = DH::BindGroup::Create(atmosphereLayout, {
					DH::BindGroupEntry::UniformBuffer(0, precomputer->GetUniformBuffer()),
					DH::BindGroupEntry::TextureSampler(1, precomputer->GetTransmittanceTex(), context.clampSampler),
					DH::BindGroupEntry::TextureSampler(2, precomputer->GetScatteringTex(), context.clampSampler) });
			}

			// Poll every map (each upload happens on the call that sees it finish loading)
			bool mapsChanged = isNew;
			for (const sRef<AsyncTexture>* map : { &planet->GetDiffuseMap(), &planet->GetSpecularMap(), &planet->GetCloudsMap(),
												   &planet->GetNightMap(), &planet->GetRingsShadowMap() })
			{
				if (*map != nullptr && (*map)->Poll())
					mapsChanged = true;
			}
			if (mapsChanged)
				UpdateMapBindings(*planet, bindings);
		}
	}

	void PlanetNewRenderer::RenderRings(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const float cameraYaw, const bool isBack)
	{
		// The half ring is turned to face the camera, so the back half is behind the planet
		const float yaw = isBack ? -cameraYaw - 180.0f : -cameraYaw;
		const glm::mat4 rotMatrix = glm::rotate(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

		bool pipelineBound = false;
		for (const auto& [distance, index] : distanceCache)
		{
			const PlanetNew& planet = *planets[index];
			const auto it = planetBindings.find(&planet);
			if (it == planetBindings.end() || it->second.ringGroup == nullptr)
				continue;

			if (!pipelineBound)
			{
				cmd.BindPipeline(ringsPipeline.Get(polygonMode));
				pipelineBound = true;
			}

			const RingPushConstants pushConstants{
				planet.transformationMatrix * rotMatrix * RING_ROTATION_CORRECTION,
				glm::vec4(RINGS_OUTER_RAD - RINGS_INNER_RAD, RINGS_PLANET_RADIUS, RINGS_INV_SHADOW_SOFTNESS, 0.0f)
			};
			cmd.BindGroup(1, *it->second.ringGroup);
			cmd.PushConstants(pushConstants);
			ringsMesh->Draw(cmd);
		}
	}

	void PlanetNewRenderer::Render(DH::CommandList& cmd, const DH::PolygonMode polygonMode, const float cameraYaw)
	{
		if (planets.empty()) return;

		for (const sRef<PlanetNew>& planet : planets)
			planet->RecalculateTransformation();

		RenderRings(cmd, polygonMode, cameraYaw, true);

		cmd.BindPipeline(surfacePipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const PlanetNew& planet = *planets[index];
			const auto it = planetBindings.find(&planet);
			if (it == planetBindings.end())
				continue;
			const PlanetBindings& bindings = it->second;
			const SV::Atmosphere::ProcessedAtmoData& atmoData = planet.GetAtmosphere()->GetAtmosphericData();

			// The sphere is drawn at the top of the atmosphere; the surface is found in the shader
			const glm::mat4 skyTransformation = planet.transformationMatrix * glm::scale(glm::vec3(atmoData.topRadius / atmoData.bottomRadius));
			const glm::mat3 normalMatrix = DH::Maths::CalculateNormalMatrix(planet.transformationMatrix);

			const PlanetPushConstants pushConstants{
				skyTransformation,
				{ glm::vec4(normalMatrix[0], planet.GetAtmosphere()->GetSurfaceAtmoIntensity()),
				  glm::vec4(normalMatrix[1], (float)bindings.mapFlags),
				  glm::vec4(normalMatrix[2], 0.0f) }
			};

			cmd.BindGroup(1, *bindings.atmosphereGroup);
			cmd.BindGroup(2, *bindings.mapsGroup);
			cmd.PushConstants(pushConstants);
			planetMesh->Draw(cmd);
		}

		RenderRings(cmd, polygonMode, cameraYaw, false);
	}

#pragma endregion
}
