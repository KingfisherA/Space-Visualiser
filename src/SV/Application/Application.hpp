#pragma once
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

namespace DH
{
	class Buffer;
	class GraphicsPipeline;
}

namespace SV
{
	class CelestrialBody;
	class GalaxyRenderer;
	enum class GalaxyType;
	class StarRenderer;
	class PlanetRenderer;
	class SplashScreen;

	namespace Universe
	{
		class PlanetarySystem;
	}

	// Space Visualiser: renders a planetary system into Dahlia's ImGui viewport. The scene loads over
	// the first few frames, behind a splash screen. The galaxy test shows a galaxy (the Milky Way,
	// the Whirlpool or Andromeda) in place of the solar system.
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

		// Where the camera is and what it is doing, kept for the view not being shown (the solar system's
		// during the galaxy test, and the galaxy's otherwise)
		struct CameraState
		{
			DH::Camera3D camera;
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

		// Shows the galaxy in place of the solar system (or back again), swapping the camera to the
		// other view's
		void SetGalaxyTest(bool enabled);
		// Shows another galaxy: its parameters and stars, from its first view
		void SetGalaxyType(GalaxyType type);
		// A galaxy's first view (and "Reset camera"): the whole galaxy, as it is usually seen
		[[nodiscard]] static CameraState GetGalaxyOverview(GalaxyType type);
		void RenderGalaxyTest(DH::CommandList& cmd, FrameUniforms& frame);
		void RenderGalaxyControls();
		// Marks the galaxy's centre, companions and (in the Milky Way) the Sun over the viewport
		// during the galaxy test (inside its ImGui window)
		void DrawGalaxyLabels();

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

		// The galaxy test: only the galaxy is drawn, and the camera works in parsecs around the
		// galactic centre. The renderer is created the first time the test is shown.
		bool showGalaxyTest = false;
		GalaxyType galaxyType{}; // The Milky Way
		uRef<GalaxyRenderer> galaxyRenderer;
		CameraState otherCameraState = GetGalaxyOverview(galaxyType);

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

		// Smooth camera travel to a body ("Go to"), which the camera then follows
		bool isTravelling = false;
		sRef<CelestrialBody> travelBody;
		// Where travelBody was when the camera last moved towards it, so the camera moves with it
		glm::dvec3 travelBodyPosition{ 0.0 };
		float travelZoom = 0.0f;
		sRef<CelestrialBody> followBody;
	};
}
