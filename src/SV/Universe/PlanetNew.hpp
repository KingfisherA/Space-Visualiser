#pragma once
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Atmosphere/Atmosphere.hpp"
#include "SV/Graphics/AsyncTexture.hpp"
#include "SV/Graphics/ModalPipeline.hpp"
#include "CelestrialBody.hpp"

namespace DH
{
	class BindGroup;
	class BindGroupLayout;
	class CommandList;
}

namespace SV
{
	struct SceneContext;

#pragma region Planets

	class PlanetNew : public CelestrialBody
	{
	public:
		PlanetNew(const double massIn, const sRef<SV::Atmosphere::IAtmosphere>& atmoIn) :
			CelestrialBody(CelestrialBodyType::Planet, massIn), planetAtmo(atmoIn) {}

		// Any map may be nullptr: a planet without a diffuse map is drawn black
		[[nodiscard]] const sRef<AsyncTexture>& GetDiffuseMap() const noexcept { return diffuseMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetSpecularMap() const noexcept { return specularMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetCloudsMap() const noexcept { return cloudsMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetNightMap() const noexcept { return nightMap; }
		// Planets with a ring map are drawn with rings, which also shadow the surface
		[[nodiscard]] const sRef<AsyncTexture>& GetRingsShadowMap() const noexcept { return ringShadowMap; }

		[[nodiscard]] const sRef<SV::Atmosphere::IAtmosphere>& GetAtmosphere() const noexcept { return planetAtmo; }

	protected:
		sRef<AsyncTexture> diffuseMap;
		sRef<AsyncTexture> specularMap;
		sRef<AsyncTexture> cloudsMap;
		sRef<AsyncTexture> nightMap;
		sRef<AsyncTexture> ringShadowMap;

		sRef<SV::Atmosphere::IAtmosphere> planetAtmo;
	};

	class Mercury : public PlanetNew
	{
	public:
		Mercury();
	};

	class Venus : public PlanetNew
	{
	public:
		Venus();
	};

	class Earth : public PlanetNew
	{
	public:
		Earth();
	};

	class Mars : public PlanetNew
	{
	public:
		Mars();
	};

	class Jupiter : public PlanetNew
	{
	public:
		Jupiter();
	};

	class Saturn : public PlanetNew
	{
	public:
		Saturn();
	};

	class Titan : public PlanetNew
	{
	public:
		Titan();
	};

#pragma endregion

	// Draws planets: the surface with its atmosphere in one pass over a sphere, and rings as two
	// camera facing halves (back half before the planet, front half after).
	class PlanetNewRenderer final : public CelestrialRenderer<SV::PlanetNew>
	{
	public:
		explicit PlanetNewRenderer(const SceneContext& context);
		~PlanetNewRenderer();

		// Call each frame before the scene render pass: precomputes new atmospheres and picks up
		// textures that have finished loading
		void Prepare(DH::CommandList& cmd);

		// Must be inside the scene render pass with the frame bind group bound at set 0
		void Render(DH::CommandList& cmd, DH::PolygonMode polygonMode, float cameraYaw);

	private:
		// GPU state of one planet
		struct PlanetBindings
		{
			sRef<DH::BindGroup> atmosphereGroup; // Set 1 of the surface pipeline
			sRef<DH::BindGroup> mapsGroup;       // Set 2 of the surface pipeline
			sRef<DH::BindGroup> ringGroup;       // Set 1 of the rings pipeline, if the planet has rings
			int mapFlags = 0;                    // PLANET_HAS_* in Shaders/Include/PlanetPushConstants.glsl
		};

		void UpdateMapBindings(const PlanetNew& planet, PlanetBindings& bindings);
		void RenderRings(DH::CommandList& cmd, DH::PolygonMode polygonMode, float cameraYaw, bool isBack);

		std::vector<sRef<SV::PlanetNew>>& planets = CelestrialRenderer<SV::PlanetNew>::celestrialBodies;

		const SceneContext& context;

		sRef<DH::GPUMesh> planetMesh;
		sRef<DH::GPUMesh> ringsMesh;

		sRef<DH::BindGroupLayout> atmosphereLayout;
		sRef<DH::BindGroupLayout> mapsLayout;
		sRef<DH::BindGroupLayout> ringLayout;

		ModalPipeline surfacePipeline;
		ModalPipeline ringsPipeline;

		std::unordered_map<const PlanetNew*, PlanetBindings> planetBindings;
		// Precomputers whose temporary resources can be freed once their commands have run
		std::vector<sRef<SV::Atmosphere::AtmospherePrecomputer>> finishedPrecomputers;
	};
}
