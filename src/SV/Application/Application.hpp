#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Dahlia/Application/GraphicsApp.h"
#include "Dahlia/Graphics/GPUMesh.h"
#include "Dahlia/Scene/Camera3D.h"
#include "Dahlia/Utilities/Memory.h"
#include "SV/Graphics/ModalPipeline.hpp"
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
	class PlanetNewRenderer;

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
		void MoveTo(const CelestrialBody& body, float zoom);
		// Zoom in (positive) or out by a fraction of the current distance
		void ZoomBy(float amount);
		void RenderGrid(DH::CommandList& cmd);
		void RenderCentreMarker(DH::CommandList& cmd);
		void RenderControlsPanel();

		DH::Camera3D camera;
		glm::mat4 projectionMatrix{ 1.0f };

		SceneContext sceneContext;
		sRef<DH::Buffer> frameUniformBuffer;

		sRef<DH::GPUMesh> gridMesh;
		sRef<DH::GraphicsPipeline> gridPipeline;
		sRef<DH::GPUMesh> markerMesh;
		ModalPipeline markerPipeline;

		sRef<StarRenderer> starRenderer;
		sRef<PlanetNewRenderer> planetRenderer;
		uRef<Universe::PlanetarySystem> solarSystem;
		std::vector<NamedBody> namedBodies;

		// The body that lights the scene (the Sun)
		sRef<CelestrialBody> lightSource;

		// Scene settings
		float exposure = 10.0f;
		DH::PolygonMode polygonMode = DH::PolygonMode::Solid;
		double animationTime = 0.0;
		bool simulateGravity = true;

		// Whether the cursor is over the viewport window (updated each ImGui frame)
		bool isViewportHovered = false;

		// Smooth camera travel to a body ("Go to")
		bool isTravelling = false;
		glm::vec3 travelTarget{ 0.0f };
		float travelZoom = 0.0f;
	};
}
