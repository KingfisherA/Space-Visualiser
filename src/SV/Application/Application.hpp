#pragma once
#include <string>
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
	class StarRenderer;
	class PlanetRenderer;

	namespace Universe
	{
		class PlanetarySystem;
	}

	// Space Visualiser: renders a planetary system into Dahlia's ImGui viewport
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
		};

		void UpdateProjectionMatrix();
		void UpdateCamera(float deltaTime);
		// Travel to the body, then follow it
		void MoveTo(const sRef<CelestrialBody>& body, float zoom);
		void StopFollowing();
		// The closest zoom allowed: outside the body being followed
		[[nodiscard]] float GetMinZoom() const;
		// Zoom in (positive) or out by a fraction of the current distance
		void ZoomBy(float amount);
		void RenderGrid(DH::CommandList& cmd);
		void RenderControlsPanel();
		// Names the bodies over the viewport (inside its ImGui window)
		void DrawLabels();
		// Marks the camera's orbit centre over the viewport (inside its ImGui window)
		void DrawCrosshair();

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

		// Whether the cursor is over the viewport window (updated each ImGui frame)
		bool isViewportHovered = false;

		// Smooth camera travel to a body ("Go to"), which the camera then follows
		bool isTravelling = false;
		sRef<CelestrialBody> travelBody;
		float travelZoom = 0.0f;
		sRef<CelestrialBody> followBody;
	};
}
