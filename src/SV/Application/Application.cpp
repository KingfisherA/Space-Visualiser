#include "Application.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>

#include "Dahlia/Events/KeyEvents.h"
#include "Dahlia/Events/MouseEvents.h"
#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Maths/Geometry.h"
#include "Dahlia/Platform/GPUContext.h"
#include "SV/Graphics/AsyncTexture.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "SV/Platform/Trackpad.hpp"
#include "SV/Universe/Planetary System.h"

namespace SV
{
	namespace
	{
		// Mirrors the push constants in Shaders/Grid.vert
		struct GridPushConstants
		{
			glm::mat4 transform;
		};

		// Mirrors the push constants in Shaders/Marker.vert and Marker.frag
		struct MarkerPushConstants
		{
			glm::mat4 transform;
			glm::vec4 colour;
		};

		constexpr float FOV = 45.0f;
		constexpr float NEAR_PLANE = 0.1f;
		constexpr float FAR_PLANE = 1500.0f;

		constexpr float GRID_SIZE = 1000.0f;
		constexpr unsigned int GRID_LINES = 1000;

		constexpr float DEFAULT_ZOOM = 60.0f;
		constexpr float MIN_ZOOM = 0.2f;
		constexpr float MAX_ZOOM = FAR_PLANE * 0.5f;

		// Camera input
		constexpr float WHEEL_ZOOM_STEP = 0.1f;      // Fraction of the distance per wheel notch
		constexpr float TRACKPAD_ZOOM_STEP = 0.02f;  // Per point of two finger scrolling
		constexpr float PINCH_ZOOM_SCALE = 2.0f;     // Per unit of pinch magnification
		constexpr float TRACKPAD_ORBIT_SCALE = 1.0f; // Relative to dragging the same distance
		// Pan speed per unit of zoom distance, so panning moves the view by a similar amount at
		// any distance
		constexpr float PAN_ACCELERATION_PER_ZOOM = 6.0f;
		constexpr float GO_TO_ZOOM = 6.0f;
		// Fraction of the remaining distance covered each 1/60 s when travelling to a body
		constexpr float TRAVEL_RATE = 0.11f;

		constexpr double ANIMATION_SPEED = 0.2;
		constexpr double MAX_FRAME_TIME = 0.1;
		// Simulated seconds per real second for the gravity simulation
		constexpr double SIMULATION_SPEED = 500.0;

		// A line list grid on the XZ plane, from (0, 0, 0) to (scale, 0, scale)
		DH::Mesh_F32 CreateGridLines(float scale, unsigned int lines)
		{
			const uint32_t rowSize = lines + 1;
			DH::Mesh_F32 mesh(3, (size_t)rowSize * rowSize * 3, 0, 0, (size_t)lines * lines * 8);
			std::vector<float>& vertices = mesh.GetVertexArray();
			std::vector<uint32_t>& indices = mesh.GetIndicesArray();

			size_t v = 0;
			for (uint32_t j = 0; j <= lines; ++j)
			{
				for (uint32_t i = 0; i <= lines; ++i)
				{
					vertices[v++] = ((float)i / (float)lines) * scale;
					vertices[v++] = 0.0f;
					vertices[v++] = ((float)j / (float)lines) * scale;
				}
			}

			size_t k = 0;
			for (uint32_t j = 0; j < lines; ++j)
			{
				for (uint32_t i = 0; i < lines; ++i)
				{
					const uint32_t row1 = j * rowSize;
					const uint32_t row2 = (j + 1) * rowSize;
					for (uint32_t index : { row1 + i, row1 + i + 1, row1 + i + 1, row2 + i + 1,
											row2 + i + 1, row2 + i, row2 + i, row1 + i })
						indices[k++] = index;
				}
			}
			return mesh;
		}
	}

	Application::Application() :
		DH::GraphicsApp("Space Visualiser", 1280, 800,
						DH::FramebufferSpecification{ 800, 600, 1,
													  { DH::FramebufferAttachment(DH::Format::RGBA8),
														DH::FramebufferAttachment(DH::Format::D24S8) } })
	{
	}

	Application::~Application() = default;

	bool Application::OnInit()
	{
		GRAPHICS_APP_INIT_IMGUI_CTX

		mRenderViewport = true;
		mViewportName = "Viewport";

		std::cout << "Space Visualiser running on " << mVersionStr << " (" << mVendorStr << ")\n";
		std::cout << "Textures are loaded from \"" << GetTextureDirectory().string() << "\"\n";

		camera.SetCentrePosition(glm::vec3(0.0f));
		camera.SetZoom(DEFAULT_ZOOM);
		Trackpad::Install();
		UpdateProjectionMatrix();

		// Shared scene resources
		sceneContext.targetLayout = mViewportFramebuffer->GetLayout();

		const FrameUniforms frameUniforms{};
		frameUniformBuffer = DH::Buffer::Create({ sizeof(FrameUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &frameUniforms);
		sceneContext.frameLayout = DH::BindGroupLayout::Create(
			{ { 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics } });
		sceneContext.frameGroup = DH::BindGroup::Create(sceneContext.frameLayout,
			{ DH::BindGroupEntry::UniformBuffer(0, frameUniformBuffer) });

		DH::SamplerDesc mapSampler;
		mapSampler.mipmapMode = DH::MipmapMode::Linear;
		mapSampler.addressU = DH::AddressMode::Repeat;
		mapSampler.addressV = DH::AddressMode::Repeat;
		sceneContext.mapSampler = DH::Sampler::Create(mapSampler);

		DH::SamplerDesc ringSampler;
		ringSampler.mipmapMode = DH::MipmapMode::Linear;
		ringSampler.addressU = DH::AddressMode::ClampToBorder;
		ringSampler.addressV = DH::AddressMode::ClampToBorder;
		ringSampler.borderColour = DH::BorderColour::TransparentBlack;
		sceneContext.ringSampler = DH::Sampler::Create(ringSampler);

		sceneContext.clampSampler = DH::Sampler::Create(DH::SamplerDesc{});

		const unsigned char black[4] = { 0, 0, 0, 255 };
		sceneContext.blackTexture = DH::Texture2D::Create({ 1, 1 }, black, DH::Format::RGBA8);

		// Grid
		gridMesh = DH::GPUMesh::Create(CreateGridLines(GRID_SIZE, GRID_LINES));
		DH::GraphicsPipelineDesc gridDesc;
		gridDesc.vertexShader = Shaders::Create("Grid.vert");
		gridDesc.fragmentShader = Shaders::Create("Grid.frag");
		gridDesc.vertexBindings = gridMesh->GetVertexBindings();
		gridDesc.vertexAttributes = gridMesh->GetVertexAttributes();
		gridDesc.topology = DH::PrimitiveTopology::Lines;
		// Faded transparent lines that never hide what is behind them
		gridDesc.depth = { true, false, DH::CompareOp::Less };
		gridDesc.blend = DH::BlendPreset::Alpha;
		gridDesc.targetLayout = sceneContext.targetLayout;
		gridDesc.bindGroupLayouts = { sceneContext.frameLayout };
		gridDesc.pushConstantSize = sizeof(GridPushConstants);
		gridPipeline = DH::GraphicsPipeline::Create(gridDesc);

		// Marker at the camera's orbit centre
		const DH::Mesh_F32 markerMeshData = DH::Geometry::CreateQuadsphere(0.1f, 20);
		markerMesh = DH::GPUMesh::Create(markerMeshData);
		DH::GraphicsPipelineDesc markerDesc;
		markerDesc.vertexShader = Shaders::Create("Marker.vert");
		markerDesc.fragmentShader = Shaders::Create("Marker.frag");
		markerDesc.vertexBindings = { markerMesh->GetVertexBindings()[0], markerMesh->GetVertexBindings()[1] };
		markerDesc.vertexAttributes = { markerMesh->GetVertexAttributes()[0], markerMesh->GetVertexAttributes()[1] };
		markerDesc.raster.cullMode = DH::CullMode::Back;
		markerDesc.depth = { true, true, DH::CompareOp::Less };
		markerDesc.blend = DH::BlendPreset::Alpha;
		markerDesc.targetLayout = sceneContext.targetLayout;
		markerDesc.bindGroupLayouts = { sceneContext.frameLayout };
		markerDesc.pushConstantSize = sizeof(MarkerPushConstants);
		markerPipeline = ModalPipeline(markerDesc);

		// The solar system
		starRenderer = make<StarRenderer>(sceneContext);
		planetRenderer = make<PlanetNewRenderer>(sceneContext);
		solarSystem = umake<Universe::PlanetarySystem>(starRenderer, planetRenderer);

		const sRef<PresetStars::Sun> sun = make<PresetStars::Sun>(glm::vec3(0.0f, 0.0f, 0.0f));
		sun->Scale(glm::vec3{ 15.0f });
		solarSystem->Add(sun);

		const sRef<Earth> earth = make<Earth>();
		earth->Translate(glm::vec3(20.0f, 0.0f, 0.0f));
		earth->SetVelocity({ 0.0, 24000, 0.0 });
		solarSystem->Add(earth);

		namedBodies = { { "Sun", sun }, { "Earth", earth } };

		return true;
	}

	void Application::OnDestroy()
	{
		Trackpad::Uninstall();

		// GPU resources must be released before Dahlia destroys the device
		DH::GPUContext::Get()->WaitIdle();
		namedBodies.clear();
		solarSystem.reset();
		planetRenderer.reset();
		starRenderer.reset();
		markerPipeline = ModalPipeline();
		markerMesh.reset();
		gridPipeline.reset();
		gridMesh.reset();
		sceneContext = SceneContext{};
		frameUniformBuffer.reset();

		DH::GraphicsApp::OnDestroy();
	}

	void Application::UpdateProjectionMatrix()
	{
		const float width = (float)mViewportFramebuffer->GetWidth();
		const float height = (float)mViewportFramebuffer->GetHeight();
		if (width <= 0.0f || height <= 0.0f)
			return;
		projectionMatrix = glm::perspective(glm::radians(FOV), width / height, NEAR_PLANE, FAR_PLANE);
	}

	void Application::OnViewportResize()
	{
		GraphicsApp::OnViewportResize();
		UpdateProjectionMatrix();
	}

	void Application::MoveTo(const CelestrialBody& body, const float zoom)
	{
		travelTarget = body.GetTranslation();
		travelZoom = zoom;
		isTravelling = true;
	}

	void Application::UpdateCamera(const float deltaTime)
	{
		// W/A/S/D move the camera's centre while the viewport has focus
		if (mIsViewportFocused && !ImGui::GetIO().WantCaptureKeyboard &&
			(mKeyDown == DH::KeyCode::K_W || mKeyDown == DH::KeyCode::K_A ||
			 mKeyDown == DH::KeyCode::K_S || mKeyDown == DH::KeyCode::K_D))
		{
			isTravelling = false;
			camera.UpdateMovement(glm::vec2(0.0f), DH::MouseButton::None, mKeyDown, mKeyMods);
		}

		// Pinch to zoom (consumed even when not over the viewport so it doesn't build up)
		const float magnification = Trackpad::ConsumeMagnification();
		if (magnification != 0.0f && isViewportHovered)
			ZoomBy(magnification * PINCH_ZOOM_SCALE);

		camera.SetPosAcc(PAN_ACCELERATION_PER_ZOOM * std::max(camera.GetZoom(), MIN_ZOOM));

		if (isTravelling)
		{
			const float t = 1.0f - std::pow(1.0f - TRAVEL_RATE, deltaTime * 60.0f);
			const glm::vec3 centre = glm::mix(camera.GetCentrePosition(), travelTarget, t);
			const float zoom = glm::mix(camera.GetZoom(), travelZoom, t);

			constexpr float THRESHOLD = 0.001f;
			if (glm::distance2(centre, travelTarget) < THRESHOLD * THRESHOLD && std::abs(travelZoom - zoom) < THRESHOLD)
			{
				isTravelling = false;
				camera.SetCentrePosition(travelTarget);
				camera.SetZoom(travelZoom);
			}
			else
			{
				camera.SetCentrePosition(centre);
				camera.SetZoom(zoom);
			}
		}

		camera.UpdateCamera(deltaTime);
	}

	void Application::OnRender(DH::CommandList& cmd)
	{
		// The first frame's delta covers start up, and the atmosphere precompute makes a long frame;
		// clamp so the camera and simulation don't jump
		const double frameTime = std::min(mDeltaTime, MAX_FRAME_TIME);
		const float deltaTime = (float)frameTime;
		UpdateCamera(deltaTime);
		animationTime += ANIMATION_SPEED * frameTime;

		if (simulateGravity)
			solarSystem->Update(frameTime * SIMULATION_SPEED);

		const glm::mat4& view = camera.GetViewMatrix();
		FrameUniforms frame;
		frame.viewProjection = projectionMatrix * view;
		frame.view = view;
		frame.projection = projectionMatrix;
		frame.cameraPosition = glm::vec4(camera.GetActualPosition(), 1.0f);
		frame.cameraCentre = glm::vec4(camera.GetCentrePosition(), 1.0f);
		frame.sun = glm::vec4(lightPosition, exposure);
		frame.times = glm::vec4((float)animationTime, (float)animationTime * 0.05f, 0.0f, 0.0f);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// Atmosphere precomputation and texture uploads happen outside the scene pass
		planetRenderer->Prepare(cmd);

		const glm::vec3& cameraPosition = camera.GetActualPosition();
		planetRenderer->PopulateDistanceCache(cameraPosition);
		starRenderer->PopulateDistanceCache(cameraPosition);

		DH::RenderPassDesc pass;
		pass.target = mViewportFramebuffer.get();
		pass.clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.BeginRenderPass(pass);

		cmd.BindGroup(0, *sceneContext.frameGroup);

		RenderGrid(cmd);

		// Draw the furthest kind of body first so blending (coronas, atmospheres) composites correctly
		auto RenderPlanets = [&]() { planetRenderer->Render(cmd, polygonMode, camera.GetYaw()); };
		auto RenderStars = [&]() { starRenderer->Render(cmd, polygonMode); };
		if (planetRenderer->GetClosestDistance() <= starRenderer->GetClosestDistance())
		{
			RenderStars();
			RenderPlanets();
		}
		else
		{
			RenderPlanets();
			RenderStars();
		}

		RenderCentreMarker(cmd);

		cmd.EndRenderPass();
	}

	void Application::RenderGrid(DH::CommandList& cmd)
	{
		cmd.BindPipeline(*gridPipeline);
		cmd.PushConstants(GridPushConstants{ glm::translate(glm::vec3(-GRID_SIZE * 0.5f, 0.0f, -GRID_SIZE * 0.5f)) });
		gridMesh->Draw(cmd);
	}

	void Application::RenderCentreMarker(DH::CommandList& cmd)
	{
		cmd.BindPipeline(markerPipeline.Get(polygonMode));
		cmd.PushConstants(MarkerPushConstants{ glm::translate(camera.GetCentrePosition()), glm::vec4(1.0f, 1.0f, 1.0f, 0.85f) });
		markerMesh->Draw(cmd);
	}

	void Application::OnImGuiRender()
	{
		// GraphicsApp drew the viewport window this frame; reopen it to ask whether the cursor is
		// over it, so scrolling and pinching work without clicking the viewport first
		if (ImGui::Begin(mViewportName.c_str()))
			isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
		else
			isViewportHovered = false;
		ImGui::End();

		RenderControlsPanel();
	}

	void Application::RenderControlsPanel()
	{
		if (!ImGui::Begin("Space Visualiser"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Exposure (-/=)", &exposure, 0.1f, 0.0f, 100.0f);

			int mode = (int)polygonMode;
			const char* modes[] = { "Solid (1)", "Wireframe (2)", "Points (3)" };
			if (ImGui::Combo("Render style", &mode, modes, 3))
				polygonMode = (DH::PolygonMode)mode;

			ImGui::Checkbox("Simulate gravity", &simulateGravity);
			ImGui::DragFloat3("Light position", &lightPosition[0], 0.1f);
		}

		if (ImGui::CollapsingHeader("Bodies", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const NamedBody& namedBody : namedBodies)
			{
				const glm::vec3& position = namedBody.body->GetTranslation();
				ImGui::PushID(namedBody.name.c_str());
				if (ImGui::Button("Go to"))
					MoveTo(*namedBody.body, GO_TO_ZOOM);
				ImGui::SameLine();
				ImGui::Text("%s (%.1f, %.1f, %.1f)", namedBody.name.c_str(), position.x, position.y, position.z);
				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const glm::vec3& centre = camera.GetCentrePosition();
			ImGui::Text("Centre: X=%.2f, Y=%.2f, Z=%.2f", centre.x, centre.y, centre.z);
			ImGui::Text("Zoom: %.2f, Pitch: %.1f, Yaw: %.1f", camera.GetZoom(), camera.GetPitch(), camera.GetYaw());
			if (ImGui::Button("Reset camera"))
			{
				travelTarget = glm::vec3(0.0f);
				travelZoom = DEFAULT_ZOOM;
				isTravelling = true;
			}
			ImGui::TextWrapped("Trackpad: two finger scroll to orbit, pinch or Cmd + scroll to zoom, Shift + scroll to pan.");
			ImGui::TextWrapped("Mouse: drag to orbit, right drag or Option + drag to pan, wheel or Shift + drag to zoom.");
			ImGui::TextWrapped("W/A/S/D: move.");
		}

		if (ImGui::CollapsingHeader("Renderer"))
		{
			ImGui::Text("API: %s", mVersionStr.c_str());
			ImGui::Text("Vendor: %s", mVendorStr.c_str());
			ImGui::Text("Frame rate: %.0f fps (%.1f ms)", ImGui::GetIO().Framerate, ImGui::GetIO().DeltaTime * 1000.0f);
		}

		ImGui::End();
	}

	bool Application::OnKeyPressed(DH::KeyPressedEvent& e)
	{
		GraphicsApp::OnKeyPressed(e);
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;

		switch (e.GetKeyCode())
		{
		case DH::KeyCode::K_MINUS:
			exposure = std::max(exposure - 0.5f, 0.0f);
			break;
		case DH::KeyCode::K_EQUAL:
			exposure += 0.5f;
			break;
		case DH::KeyCode::K_1:
			polygonMode = DH::PolygonMode::Solid;
			break;
		case DH::KeyCode::K_2:
			polygonMode = DH::PolygonMode::Lines;
			break;
		case DH::KeyCode::K_3:
			polygonMode = DH::PolygonMode::Points;
			break;
		default:
			break;
		}
		return false;
	}

	bool Application::OnMouseMove(DH::MouseMoveEvent& e)
	{
		GraphicsApp::OnMouseMove(e);

		if (mIsViewportFocused && mMouseButtonDown != DH::MouseButton::None)
		{
			isTravelling = false;
			const ImGuiIO& io = ImGui::GetIO();
			const glm::vec2 delta = mDeltaMousePos / glm::vec2((float)mWindow->GetWidth(), (float)mWindow->GetHeight());

			// Option + drag pans, for trackpads where a right drag is awkward
			DH::MouseButton button = mMouseButtonDown;
			if (button == DH::MouseButton::Left && io.KeyAlt)
				button = DH::MouseButton::Right;

			camera.UpdateMovement(delta, button, mKeyDown, io.KeyShift ? DH::MOD_BIT_SHIFT : 0);
		}
		return true;
	}

	void Application::ZoomBy(const float amount)
	{
		// Exponential, so each step covers the same fraction of the distance at any zoom
		isTravelling = false;
		camera.SetZoom(std::clamp(camera.GetZoom() * std::exp(-amount), MIN_ZOOM, MAX_ZOOM));
	}

	bool Application::OnMouseScroll(DH::MouseScrollEvent& e)
	{
		// GraphicsApp's scroll speed is derived from the time between events, which explodes with
		// the rapid stream of small events a trackpad sends, so the raw offsets are used instead
		if (!isViewportHovered)
			return false;

		const ImGuiIO& io = ImGui::GetIO();
		const glm::vec2 offset(e.GetXOffset(), e.GetYOffset());

		if (!Trackpad::LastScrollWasPrecise())
		{
			// Mouse wheel: zoom a fixed fraction per notch
			ZoomBy(offset.y * WHEEL_ZOOM_STEP);
			return true;
		}

		// Trackpad. GLFW reports precise deltas at a tenth of a point; convert back to points and
		// normalise by the window size like mouse drags
		isTravelling = false;
		const glm::vec2 delta = offset * 10.0f / glm::vec2((float)mWindow->GetWidth(), (float)mWindow->GetHeight());
		if (io.KeySuper || io.KeyCtrl)
			ZoomBy(offset.y * TRACKPAD_ZOOM_STEP);
		else if (io.KeyShift)
			camera.UpdateMovement(delta, DH::MouseButton::Right);
		else
			camera.UpdateMovement(delta * TRACKPAD_ORBIT_SCALE, DH::MouseButton::Left);
		return true;
	}
}
