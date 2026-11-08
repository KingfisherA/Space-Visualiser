#pragma once
#include <array>
#include <future>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Application/GraphicsApp.h"
#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Graphics/GraphicsTypes.h"
#include "Dahlia/Scene/Camera3D.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Universe/Catalogue.hpp"
#include "SV/Universe/UniverseRenderer.hpp"

namespace DH
{
	class Buffer;
	class GraphicsPipeline;
}

namespace SV
{
	class CelestrialBody;
	class CompactObjectRenderer;
	class GalaxyRenderer;
	enum class GalaxyType;
	class NebulaRenderer;
	enum class NebulaType;
	class StarRenderer;
	class PlanetRenderer;
	class SplashScreen;
	class StarSystemRenderer;
	enum class StarSystemType;

	namespace Universe
	{
		class PlanetarySystem;
	}

	// Space Visualiser: renders a planetary system into Dahlia's ImGui viewport, inside the universe
	// of the star, galaxy, nebula and pulsar catalogues (seen as the camera zooms out past the solar
	// system). The scene loads over the first few frames, behind a splash screen. The galaxy test shows a galaxy (the Milky Way,
	// or one of the galaxies with presets in Galaxy.hpp) in place of the solar system, the black hole and pulsar tests a
	// black hole or a neutron star, the nebula test a nebula, and the star test stars of each kind
	// (single, binary and triple).
	class Application final : public DH::GraphicsApp
	{
	public:
		Application();
		~Application() override;

	protected:
		bool OnInit() override;
		void OnDestroy() override;
		void OnRender(DH::CommandList& cmd) override;
		void OnImGuiRender() override;
		void OnViewportResize() override;

		bool OnKeyPressed(DH::KeyPressedEvent& e) override;
		bool OnMouseMove(DH::MouseMoveEvent& e) override;
		bool OnMouseScroll(DH::MouseScrollEvent& e) override;

	private:
		struct NamedBody
		{
			std::string name;
			sRef<CelestrialBody> body;
			std::string parent; // The planet a moon orbits
		};

		// Each slow step of loading is given its own frame, so the splash screen can show progress
		enum class LoadStage
		{
			Start,        // Showing the splash screen
			CreateScene,  // Creating the pipelines and the solar system
			Precompute,   // Precomputing the atmospheres (in PlanetRenderer::Prepare)
			LoadTextures, // Rendering, while the planet maps load
			Loaded        // The splash screen is fading out (or gone)
		};

		// What the viewport shows
		enum class SceneMode
		{
			SolarSystem,
			Galaxy,   // The galaxy test: only a galaxy, in parsecs around its centre
			BlackHole, // The black hole test: only a black hole, in Schwarzschild radii around it
			Pulsar,    // The pulsar test: only a neutron star, in Schwarzschild radii around it
			Nebula,    // The nebula test: only a nebula, in parsecs around its centre
			Stars,     // The star test: a star or star system, in solar radii
			Count
		};

		// Where the camera is and what it is doing, kept for each scene while it isn't shown
		struct CameraState
		{
			DH::Camera3D camera = DH::Camera3D();
			glm::dvec3 origin{ 0.0 };
			sRef<CelestrialBody> followBody;
			bool isTravelling = false;
			sRef<CelestrialBody> travelBody;
			float travelZoom = 0.0f;
		};

		void CreateScene();
		// Clears the viewport framebuffer, for frames before the scene is rendered
		void ClearViewport(DH::CommandList& cmd);
		void DrawSplashScreen();
		// Docks the controls panel to the left of the viewport (when there is no saved layout)
		void ApplyDefaultLayout();

		void UpdateProjectionMatrix();
		void UpdateCamera(float deltaTime);
		// Travel to the body, then follow it
		void MoveTo(const sRef<CelestrialBody>& body, float zoom);
		// Travel to a point (world coordinates), and stay there
		void MoveTo(const glm::dvec3& point, float zoom);
		void StopFollowing();
		// The closest zoom allowed: outside the body being followed (or the galaxy test's limit)
		[[nodiscard]] float GetMinZoom() const;
		[[nodiscard]] float GetMaxZoom() const;
		// Zoom in (positive) or out by a fraction of the current distance
		void ZoomBy(float amount);
		void RenderGrid(DH::CommandList& cmd);
		void RenderControlsPanel();
		// Names the bodies over the viewport (inside its ImGui window)
		void DrawLabels();
		// Clicking a body (or its label) in the viewport selects it and opens a menu for it at the
		// cursor (inside the viewport's ImGui window)
		void UpdateBodySelection();
		// The named body (an index into namedBodies) nearest the camera along the ray through a
		// point in the viewport (in [0, 1] from its top left). Small bodies are picked within a few
		// pixels of them.
		[[nodiscard]] std::optional<size_t> PickBody(glm::vec2 point) const;
		// The bodies whose shadows are drawn this frame: those that look largest from the camera's
		// centre (the body in view, its planet and neighbouring moons)
		void SetOccluders(FrameUniforms& frame) const;
		// Marks the camera's orbit centre over the viewport (inside its ImGui window)
		void DrawCrosshair();

		// The universe around the solar system (from the catalogues), drawn behind it. The solar
		// system itself is only drawn while the camera is near enough to see anything of it.
		void RenderUniverse(DH::CommandList& cmd, double deltaTime);
		void RenderUniverseControls();
		// Names the catalogue's landmarks over the viewport (the brightest, largest and best known
		// that fit), inside its ImGui window. Clicking one opens a menu to go to it. Inside the solar
		// system, only a few of the sky's brightest are named, around the bodies' own labels.
		void DrawUniverseLabels();
		// Clicking a landmark's label, or the sky near one that can be seen (where no body of the
		// solar system is), selects it and opens a menu for it. Returns whether a landmark was
		// clicked (so no body is selected by the click).
		bool UpdateLandmarkSelection();
		// The landmark that can be seen nearest a point in the viewport (screen coordinates), within
		// a few pixels of it or inside its disc, favouring the more prominent
		[[nodiscard]] std::optional<size_t> PickLandmark(glm::vec2 point) const;
		// Where a position of the universe (parsecs from the Sun) is on screen, if in front of the
		// camera
		[[nodiscard]] std::optional<glm::vec2> ProjectLandmark(const glm::dvec3& position) const;
		// The discs of the solar system's bodies on screen (centre, radius in pixels) at least a pixel
		// across, which hide the sky behind them
		[[nodiscard]] std::vector<glm::vec3> GetBodyDiscs() const;
		// How far from the Sun the camera is (world units)
		[[nodiscard]] double GetSunDistance() const;
		// Whether the solar system is drawn (the camera is near enough to it)
		[[nodiscard]] bool IsSolarSystemShown() const;
		// Travel to a landmark, at a zoom to suit it
		void GoToLandmark(const CatalogueLandmark& landmark);

		// Shows another scene, swapping the camera to where it was in that scene
		void SetSceneMode(SceneMode mode);
		// Shows another galaxy: its parameters and stars, from its first view
		void SetGalaxyType(GalaxyType type);
		// A galaxy's first view (and "Reset camera"): the whole galaxy, as it is usually seen
		[[nodiscard]] static CameraState GetGalaxyOverview(GalaxyType type);
		void RenderGalaxyTest(DH::CommandList& cmd, FrameUniforms& frame);
		void RenderGalaxyControls();
		// Marks the galaxy's centre, companions and (in the Milky Way) the Sun over the viewport
		// during the galaxy test (inside its ImGui window)
		void DrawGalaxyLabels();

		// The black hole's or pulsar's first view (and "Reset camera"): just above the disc (for
		// the black hole, as in Interstellar)
		[[nodiscard]] static CameraState GetCompactObjectOverview(SceneMode mode);
		// The renderer of the black hole or pulsar test (whichever is shown)
		[[nodiscard]] CompactObjectRenderer& GetCompactObjectRenderer() const;
		void RenderCompactObjectTest(DH::CommandList& cmd, FrameUniforms& frame, double deltaTime);
		void RenderCompactObjectControls();

		// Shows another nebula: its parameters, from its first view
		void SetNebulaType(NebulaType type);
		// A nebula's first view (and "Reset camera"): the whole nebula, as it is usually seen
		[[nodiscard]] static CameraState GetNebulaOverview(NebulaType type);
		void RenderNebulaTest(DH::CommandList& cmd, FrameUniforms& frame, double deltaTime);
		void RenderNebulaControls();
		// Names the nebula's stars over the viewport (inside its ImGui window)
		void DrawNebulaLabels();

		// Shows another star system: from its start, and its first view
		void SetStarSystemType(StarSystemType type);
		[[nodiscard]] static CameraState GetStarSystemOverview(StarSystemType type);
		void RenderStarTest(DH::CommandList& cmd, FrameUniforms& frame);
		void RenderStarControls();
		// Names the star system's bodies over the viewport (inside its ImGui window)
		void DrawStarLabels();

		// The current scene's first view (for "Reset camera" in the test scenes)
		[[nodiscard]] CameraState GetTestOverview() const;
		// The angle a pixel of the viewport subtends (radians) with a field of view (degrees)
		[[nodiscard]] float GetPixelAngle(float fieldOfView) const;

		// The camera works in render coordinates: world positions relative to cameraOrigin (kept at
		// the camera's centre each frame), which keeps them small enough for floats
		DH::Camera3D camera;
		glm::dvec3 cameraOrigin{ 0.0 };
		glm::mat4 projectionMatrix{ 1.0f };
		glm::mat4 viewProjection{ 1.0f };

		SceneContext sceneContext;
		sRef<DH::Buffer> frameUniformBuffer;

		sRef<DH::GPUMesh> gridMesh;
		sRef<DH::GraphicsPipeline> gridPipeline;

		sRef<StarRenderer> starRenderer;
		sRef<PlanetRenderer> planetRenderer;
		uRef<Universe::PlanetarySystem> solarSystem;
		std::vector<NamedBody> namedBodies;

		// The body that lights the scene (the Sun)
		sRef<CelestrialBody> lightSource;

		// Scene settings
		float exposure = 10.0f;
		DH::PolygonMode polygonMode = DH::PolygonMode::Solid;
		double animationTime = 0.0;
		bool simulateGravity = true;
		// Simulated days per real second
		float daysPerSecond = 1.0f;
		bool showGrid = true;
		bool showLabels = true;

		// The scene shown, and the camera of each scene (the one shown's is out of date). The test
		// scenes' renderers are created the first time they are shown.
		SceneMode sceneMode = SceneMode::SolarSystem;
		GalaxyType galaxyType{}; // The Milky Way
		uRef<GalaxyRenderer> galaxyRenderer;
		uRef<CompactObjectRenderer> blackHoleRenderer;
		uRef<CompactObjectRenderer> pulsarRenderer;
		NebulaType nebulaType{}; // The Crab
		uRef<NebulaRenderer> nebulaRenderer;
		StarSystemType starSystemType{}; // A red dwarf
		uRef<StarSystemRenderer> starSystemRenderer;
		// The star system's node (a body, or a pair's centre of mass) the camera follows, if any
		std::optional<size_t> starFocus;
		std::array<CameraState, (size_t)SceneMode::Count> sceneCameras{
			CameraState{}, GetGalaxyOverview(galaxyType), GetCompactObjectOverview(SceneMode::BlackHole),
			GetCompactObjectOverview(SceneMode::Pulsar), GetNebulaOverview(nebulaType), GetStarSystemOverview(starSystemType) };

		// The catalogues, parsed on a worker thread from the start, then drawn by universeRenderer
		std::future<Catalogue> pendingCatalogue;
		uRef<Catalogue> catalogue;
		uRef<UniverseRenderer> universeRenderer;
		bool showUniverse = true;
		UniverseView universeView; // This frame's
		double universeTime = 0.0;
		// The landmark search ("Find") and the landmark whose menu is open
		std::array<char, 64> landmarkFilter{};
		std::optional<size_t> selectedLandmark;
		std::vector<std::pair<glm::vec4, size_t>> landmarkLabelRects;

		// Whether the cursor is over the viewport window (updated each ImGui frame)
		bool isViewportHovered = false;

		// The labels drawn this frame (min x, min y, max x, max y in screen coordinates), and the
		// named body each is for, so a label can be clicked to select its body
		std::vector<std::pair<glm::vec4, size_t>> labelRects;
		// The body whose menu is open (an index into namedBodies)
		std::optional<size_t> selectedBody;

		LoadStage loadStage = LoadStage::Start;
		uRef<SplashScreen> splashScreen;

		// ImGui keeps a pointer to the path of its settings file
		std::string imguiIniPath;
		bool applyDefaultLayout = false;

		// Smooth camera travel to a body ("Go to"), which the camera then follows, or to a point
		bool isTravelling = false;
		sRef<CelestrialBody> travelBody;
		glm::dvec3 travelPoint{ 0.0 }; // When travelBody is null
		// Where travelBody was when the camera last moved towards it, so the camera moves with it
		glm::dvec3 travelBodyPosition{ 0.0 };
		float travelZoom = 0.0f;
		sRef<CelestrialBody> followBody;
	};
}
