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

	// A ring system's extent in the planet's equatorial plane, in equatorial radii (see
	// SV/Universe/Rings.hpp for the maps)
	struct PlanetRings
	{
		// The ring map (macroscopic particles) spans innerRadius to outerRadius
		float innerRadius = 0.0f;
		float outerRadius = 0.0f;
		// The dust map spans the planet's centre to dustOuterRadius; the dust starts at dustInnerRadius
		float dustInnerRadius = 0.0f;
		float dustOuterRadius = 0.0f;
	};

	// A planet's size is its scale (world units per equatorial radius, the same on every axis); its
	// shape is fixed by the polar ratio
	class Planet : public CelestrialBody
	{
	public:
		Planet(const double massIn, const sRef<SV::Atmosphere::IAtmosphere>& atmoIn) :
			CelestrialBody(CelestrialBodyType::Planet, massIn), planetAtmo(atmoIn) {}

		// Any map may be nullptr: a planet without a diffuse map is drawn black
		[[nodiscard]] const sRef<AsyncTexture>& GetDiffuseMap() const noexcept { return diffuseMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetSpecularMap() const noexcept { return specularMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetCloudsMap() const noexcept { return cloudsMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetNightMap() const noexcept { return nightMap; }
		// Planets with a ring map or a dust map are drawn with rings (spanning GetRings). Rings in the
		// ring map also shadow the surface.
		[[nodiscard]] const sRef<AsyncTexture>& GetRingMap() const noexcept { return ringMap; }
		[[nodiscard]] const sRef<AsyncTexture>& GetDustMap() const noexcept { return dustMap; }
		[[nodiscard]] const PlanetRings& GetRings() const noexcept { return rings; }

		[[nodiscard]] const sRef<SV::Atmosphere::IAtmosphere>& GetAtmosphere() const noexcept { return planetAtmo; }

		// Polar radius / equatorial radius: 1 for a sphere, less for an oblate spheroid
		[[nodiscard]] float GetPolarRatio() const noexcept { return polarRatio; }
		// Turns of the surface map per unit of planet animation time
		[[nodiscard]] float GetRotationSpeed() const noexcept { return rotationSpeed; }
		// Lit surfaces are shaded with the sunlight transmitted by the atmosphere (gas giants);
		// otherwise Earth's stylised shading is used
		[[nodiscard]] bool HasLitSurface() const noexcept { return litSurface; }
		// Lit surfaces: the reflectance of the brightest point of the diffuse map (a display image
		// normalised to its brightest point)
		[[nodiscard]] float GetSurfaceAlbedo() const noexcept { return surfaceAlbedo; }

	protected:
		sRef<AsyncTexture> diffuseMap;
		sRef<AsyncTexture> specularMap;
		sRef<AsyncTexture> cloudsMap;
		sRef<AsyncTexture> nightMap;
		sRef<AsyncTexture> ringMap;
		sRef<AsyncTexture> dustMap;
		PlanetRings rings;

		sRef<SV::Atmosphere::IAtmosphere> planetAtmo;

		float polarRatio = 1.0f;
		float rotationSpeed = 0.3f;
		bool litSurface = false;
		float surfaceAlbedo = 0.5f;
	};

	class Mercury : public Planet
	{
	public:
		Mercury();
	};

	class Venus : public Planet
	{
	public:
		Venus();
	};

	class Earth : public Planet
	{
	public:
		Earth();
	};

	class Mars : public Planet
	{
	public:
		Mars();
	};

	class Jupiter : public Planet
	{
	public:
		Jupiter();
	};

	class Saturn : public Planet
	{
	public:
		Saturn();
	};

	class Uranus : public Planet
	{
	public:
		Uranus();
	};

	class Neptune : public Planet
	{
	public:
		Neptune();
	};

	class Titan : public Planet
	{
	public:
		Titan();
	};

#pragma endregion

	// Draws planets: the surface with its atmosphere in one pass over a spheroid, and rings as two
	// halves (the half behind the planet before it, the half in front after).
	class PlanetRenderer final : public CelestrialRenderer<SV::Planet>
	{
	public:
		explicit PlanetRenderer(const SceneContext& context);
		~PlanetRenderer();

		// Call each frame before the scene render pass: precomputes new atmospheres and picks up
		// textures that have finished loading
		void Prepare(DH::CommandList& cmd);

		// Must be inside the scene render pass with the frame bind group bound at set 0
		void Render(DH::CommandList& cmd, DH::PolygonMode polygonMode, const glm::vec3& cameraPosition);

		// Brightness gain of the faint, dusty rings (such as Saturn's E ring and Jupiter's rings), up
		// to an optical depth of about 0.02. At 1 they have their true optical depths (about 1e-5)
		// and are hundreds of times fainter than Saturn's main rings; the default shows them as in
		// long exposures such as Cassini's "The Day the Earth Smiled".
		float diffuseRingGain = 300.0f;

	private:
		// GPU state of one planet
		struct PlanetBindings
		{
			sRef<DH::BindGroup> atmosphereGroup; // Set 1 of the surface pipeline
			sRef<DH::BindGroup> mapsGroup;       // Set 2 of the surface pipeline
			sRef<DH::BindGroup> ringGroup;       // Set 1 of the rings pipeline, if the planet has rings
			sRef<DH::BindGroup> ringShadowGroup; // Set 2 of the rings pipeline (the planet's atmosphere)
			sRef<DH::GPUMesh> ringsMesh;         // Half of the planet's rings
			sRef<DH::GPUMesh> skyMesh;           // Spheroid of the planet's shape
			int mapFlags = 0;                    // PLANET_* in Shaders/Include/PlanetPushConstants.glsl
			bool hasRingMap = false;             // Whether the ring and dust maps have loaded
			bool hasDustMap = false;
		};

		void UpdateMapBindings(const Planet& planet, PlanetBindings& bindings);
		const sRef<DH::GPUMesh>& GetSkyMesh(float polarRatio);
		void RenderRings(DH::CommandList& cmd, DH::PolygonMode polygonMode, const glm::vec3& cameraPosition, bool isBack);

		std::vector<sRef<SV::Planet>>& planets = CelestrialRenderer<SV::Planet>::celestrialBodies;

		const SceneContext& context;

		// Sky meshes by polar ratio
		std::unordered_map<float, sRef<DH::GPUMesh>> skyMeshes;

		sRef<DH::BindGroupLayout> atmosphereLayout;
		sRef<DH::BindGroupLayout> mapsLayout;
		sRef<DH::BindGroupLayout> ringLayout;
		sRef<DH::BindGroupLayout> ringShadowLayout;

		ModalPipeline surfacePipeline;
		ModalPipeline ringsPipeline;

		std::unordered_map<const Planet*, PlanetBindings> planetBindings;
		// Precomputers whose temporary resources can be freed once their commands have run
		std::vector<sRef<SV::Atmosphere::AtmospherePrecomputer>> finishedPrecomputers;
	};
}
