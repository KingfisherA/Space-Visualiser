#include "Application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>
#include <imgui_internal.h>

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
#include "SV/Application/SplashScreen.hpp"
#include "SV/Graphics/AsyncTexture.hpp"
#include "SV/Graphics/ShaderSources.hpp"
#include "SV/Platform/Trackpad.hpp"
#include "SV/Universe/Galaxy.hpp"
#include "SV/Universe/MoonData.hpp"
#include "SV/Universe/PlanetarySystem.hpp"
#include "SV/Universe/SolarSystem.hpp"

namespace SV
{
	namespace
	{
		// Mirrors the push constants in Shaders/Grid.vert and Grid.frag
		struct GridPushConstants
		{
			glm::mat4 transform;
			glm::vec4 params; // x = fade distance, y = opacity
		};

		constexpr float FOV = 45.0f;
		// The solar system spans millions of units, so the near and far planes are fitted to the scene
		// each frame: the near plane is this fraction of the distance to the nearest surface
		constexpr float NEAR_PLANE_FRACTION = 0.05f;
		constexpr float MIN_NEAR_PLANE = 1e-6f;
		// Extents of a body in its radii: its surface (with the atmosphere), and everything drawn
		// around it (rings and coronae)
		constexpr float BODY_SURFACE_EXTENT = 1.02f;
		constexpr float BODY_BOUNDING_EXTENT = 10.0f;

		// The grid lies in the ecliptic, drawn at two scales a factor of 10 apart (powers of 10
		// chosen from the zoom; the finer fades out as the camera zooms out). Each scale is a square
		// of GRID_LINES spacings around the camera's centre, fading out GRID_FADE_SPACINGS from it.
		constexpr unsigned int GRID_LINES = 100;
		constexpr float GRID_FADE_SPACINGS = 40.0f;
		// The finer scale's spacing is at most the zoom divided by this
		constexpr float GRID_SPACINGS_PER_ZOOM = 30.0f;
		constexpr float GRID_OPACITY = 0.35f;

		// About 300 m: the smallest moons are a kilometre or two across
		constexpr float MIN_ZOOM = 5e-5f;
		constexpr float MAX_ZOOM = 3.0e6f; // About 128 AU
		// Closest zoom to the body being followed, in its radii
		constexpr float MIN_ZOOM_PER_RADIUS = 1.1f;

		// Camera input
		constexpr float WHEEL_ZOOM_STEP = 0.1f;      // Fraction of the distance per wheel notch
		constexpr float TRACKPAD_ZOOM_STEP = 0.02f;  // Per point of two finger scrolling
		constexpr float PINCH_ZOOM_SCALE = 2.0f;     // Per unit of pinch magnification
		constexpr float DRAG_ZOOM_SCALE = 5.0f;      // Per window height of Shift + dragging
		constexpr float TRACKPAD_ORBIT_SCALE = 1.0f; // Relative to dragging the same distance
		// Pan speed per unit of zoom distance, so panning moves the view by a similar amount at
		// any distance
		constexpr float PAN_ACCELERATION_PER_ZOOM = 6.0f;
		// "Go to" zoom distance per unit of the body's radius
		constexpr float GO_TO_ZOOM = 6.0f;
		// Fraction of the remaining distance (and log zoom) covered each 1/60 s when travelling to a body
		constexpr float TRAVEL_RATE = 0.11f;
		// "Reset camera" zoom: the inner solar system, around the Sun
		constexpr double OVERVIEW_ZOOM = 4.0 * Universe::AU_IN_UNITS;

		// The galaxy test works in parsecs: zoom limits, the near plane as a fraction of the zoom,
		// and how far past the galactic centre anything is drawn (each galaxy's first view is in its
		// GalaxyPreset)
		constexpr float GALAXY_MIN_ZOOM = 20.0f;
		constexpr float GALAXY_MAX_ZOOM = 500000.0f;
		constexpr float GALAXY_NEAR_PLANE_FRACTION = 1e-3f;
		constexpr float GALAXY_EXTENT = 60000.0f;

		// Camera3D can only be turned by input, which it eases out over the following frames, so
		// turn it with one impulse and run the easing to its end: the angle turned is the impulse's
		// velocity times dt / (1 - decay)
		void TurnCamera(DH::Camera3D& camera, const float pitchDegrees, const float yawDegrees)
		{
			constexpr float DT = 1.0f / 60.0f;
			const float scale = (1.0f - camera.GetDecayFactor()) / (DT * camera.GetPitchYawAcc());
			camera.UpdateMovement(glm::vec2(yawDegrees * scale, -pitchDegrees * scale), DH::MouseButton::Left);
			for (int i = 0; i < 400; i++)
				camera.UpdateCamera(DT);
		}

		// The crosshair at the camera's orbit centre, in pixels: the length of each arm, the gap
		// between the arms and the centre, and the lines' thickness
		constexpr float CROSSHAIR_ARM = 8.0f;
		constexpr float CROSSHAIR_GAP = 3.0f;
		constexpr float CROSSHAIR_THICKNESS = 1.5f;

		// Body labels: a dot for bodies smaller than this on screen, and no label for bodies larger
		// than this fraction of the viewport's height. Labels that would overlap one already drawn
		// (larger bodies are labelled first) are left out.
		constexpr float LABEL_DOT_RADIUS = 2.5f;
		constexpr float LABEL_MAX_DISC_FRACTION = 0.25f;
		constexpr float LABEL_PADDING = 3.0f;

		// Selecting a body by clicking it: bodies smaller than this on screen (pixels, as a radius)
		// are picked within it, and the selected body is ringed this far outside its disc
		constexpr float PICK_RADIUS_PIXELS = 6.0f;
		constexpr float SELECTION_RING_GAP = 4.0f;
		constexpr const char* BODY_MENU = "BodyMenu";

		// The radius of everything drawn of a body's sphere: its atmosphere's top for planets and moons
		float GetOuterRadius(const CelestrialBody& body)
		{
			const float radius = body.GetScale().x;
			if (body.GetType() == CelestrialBodyType::Planet || body.GetType() == CelestrialBodyType::Moon)
			{
				const auto& atmosphere = static_cast<const Planet&>(body).GetAtmosphere()->GetAtmosphericData();
				return radius * atmosphere.topRadius / atmosphere.bottomRadius;
			}
			return radius;
		}

		// Days since J2000.0 (UTC) as a calendar date and time
		std::string FormatDate(const double days)
		{
			// From the Julian day (Fliegel and Van Flandern)
			const double julianDay = days + 2451545.0;
			const double shifted = julianDay + 0.5;
			const long z = (long)std::floor(shifted);
			const double fraction = shifted - z;
			long a = z + 32044;
			const long b = (4 * a + 3) / 146097;
			const long c = a - 146097 * b / 4;
			const long d = (4 * c + 3) / 1461;
			const long e = c - 1461 * d / 4;
			const long m = (5 * e + 2) / 153;
			const long day = e - (153 * m + 2) / 5 + 1;
			const long month = m + 3 - 12 * (m / 10);
			const long year = 100 * b + d - 4800 + m / 10;
			const long minutes = (long)std::floor(fraction * 1440.0);
			char text[64];
			std::snprintf(text, sizeof(text), "%04ld-%02ld-%02ld %02ld:%02ld UTC", year, month, day, minutes / 60, minutes % 60);
			return text;
		}

		constexpr double ANIMATION_SPEED = 0.2;
		constexpr double MAX_FRAME_TIME = 0.1;
		constexpr double SECONDS_PER_DAY = 86400.0;

		// The splash screen shows for at least this long, and stops waiting for textures after the
		// longest time (planets are drawn black until their maps load)
		constexpr double MIN_SPLASH_SECONDS = 1.5;
		constexpr double MAX_SPLASH_SECONDS = 15.0;
		// The default layout: the controls panel docked to the left of the viewport, with this
		// fraction of the window's width
		constexpr float CONTROLS_PANEL_FRACTION = 0.25f;

		// Where ImGui keeps the window layout (imgui.ini): a per-user settings directory, so it is
		// the same however Space Visualiser is launched (an app bundle is started in "/")
		std::filesystem::path GetSettingsDirectory()
		{
			auto FromEnvironment = [](const char* name) -> std::filesystem::path
			{
#ifdef _MSC_VER
#pragma warning(suppress : 4996) // getenv is fine: the value is copied straight away
#endif
				const char* value = std::getenv(name);
				return value != nullptr ? std::filesystem::path(value) : std::filesystem::path();
			};
#if defined(__APPLE__)
			const std::filesystem::path base = FromEnvironment("HOME") / "Library" / "Application Support";
			const char* name = "Space Visualiser";
#elif defined(_WIN32)
			const std::filesystem::path base = FromEnvironment("APPDATA");
			const char* name = "Space Visualiser";
#else
			std::filesystem::path base = FromEnvironment("XDG_CONFIG_HOME");
			if (base.empty() && !FromEnvironment("HOME").empty())
				base = FromEnvironment("HOME") / ".config";
			const char* name = "space-visualiser";
#endif
			return base.empty() || !base.is_absolute() ? std::filesystem::current_path() : base / name;
		}

		// The top left of the viewport image in screen coordinates, inside the viewport's ImGui window
		// (the image fills the window's content region at the framebuffer's size)
		glm::vec2 GetViewportImageOrigin()
		{
			const ImVec2 windowPos = ImGui::GetWindowPos();
			const ImVec2 start = ImGui::GetCursorStartPos();
			return { windowPos.x + start.x - ImGui::GetScrollX(), windowPos.y + start.y - ImGui::GetScrollY() };
		}

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

		// Keep the window layout in the settings directory, docking the windows on the first run
		const std::filesystem::path settingsDirectory = GetSettingsDirectory();
		std::error_code error;
		std::filesystem::create_directories(settingsDirectory, error);
		imguiIniPath = (settingsDirectory / "imgui.ini").string();
		applyDefaultLayout = !std::filesystem::exists(imguiIniPath, error);
		ImGui::GetIO().IniFilename = imguiIniPath.c_str();

		camera.SetCentrePosition(glm::vec3(0.0f));
		Trackpad::Install();

		// The scene is created and loaded over the next few frames, behind the splash screen
		splashScreen = umake<SplashScreen>();

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

		return true;
	}

	void Application::CreateScene()
	{
		// Grid
		gridMesh = DH::GPUMesh::Create(CreateGridLines(1.0f, GRID_LINES));
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

		// The solar system
		starRenderer = make<StarRenderer>(sceneContext);
		planetRenderer = make<PlanetRenderer>(sceneContext);
		solarSystem = umake<Universe::PlanetarySystem>(starRenderer, planetRenderer);

		// The Sun, planets and moons where they are today, to scale
		for (const Universe::SolarSystemBody& body : Universe::CreateSolarSystem(*solarSystem, Universe::DaysSinceJ2000Now()))
			namedBodies.push_back({ body.name, body.body, body.parent });
		lightSource = namedBodies.front().body;

		// Start at Earth
		for (const NamedBody& namedBody : namedBodies)
		{
			if (namedBody.name == "Earth")
			{
				followBody = namedBody.body;
				cameraOrigin = followBody->GetPosition();
				camera.SetZoom(GO_TO_ZOOM * followBody->GetScale().x);
			}
		}
		solarSystem->SetRenderOrigin(cameraOrigin);
		camera.UpdateCamera(0.0f);
		UpdateProjectionMatrix();
	}

	void Application::OnDestroy()
	{
		Trackpad::Uninstall();

		// GPU resources must be released before Dahlia destroys the device
		DH::GPUContext::Get()->WaitIdle();
		splashScreen.reset();
		galaxyRenderer.reset();
		namedBodies.clear();
		lightSource.reset();
		solarSystem.reset();
		planetRenderer.reset();
		starRenderer.reset();
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

		// Fit the near and far planes to the bodies (in render coordinates, around the camera), so
		// the depth buffer's precision is spent where the nearest body is
		const glm::vec3& eye = camera.GetActualPosition();
		if (showGalaxyTest)
		{
			// Nothing is depth tested, so the planes need only take in the whole galaxy
			const float nearPlane = std::max(GALAXY_NEAR_PLANE_FRACTION * camera.GetZoom(), MIN_NEAR_PLANE);
			const float farPlane = glm::length(eye + glm::vec3(cameraOrigin)) + GALAXY_EXTENT;
			projectionMatrix = glm::perspective(glm::radians(GetGalaxyPreset(galaxyType).fieldOfView), width / height, nearPlane, farPlane);
			return;
		}
		float nearest = camera.GetZoom(), farthest = camera.GetZoom();
		if (solarSystem != nullptr)
		{
			for (const sRef<CelestrialBody>& body : solarSystem->GetBodies())
			{
				const float distance = glm::distance(body->GetTranslation(), eye);
				const float radius = body->GetScale().x;
				nearest = std::min(nearest, distance - GetOuterRadius(*body) * BODY_SURFACE_EXTENT);
				farthest = std::max(farthest, distance + radius * BODY_BOUNDING_EXTENT);
			}
		}
		const float nearPlane = std::max(NEAR_PLANE_FRACTION * nearest, MIN_NEAR_PLANE);
		const float farPlane = std::max(1.5f * farthest, 10.0f * nearPlane);
		projectionMatrix = glm::perspective(glm::radians(FOV), width / height, nearPlane, farPlane);
	}

	void Application::OnViewportResize()
	{
		GraphicsApp::OnViewportResize();
		UpdateProjectionMatrix();
	}

	void Application::MoveTo(const sRef<CelestrialBody>& body, const float zoom)
	{
		travelBody = body;
		travelBodyPosition = body->GetPosition();
		travelZoom = zoom;
		isTravelling = true;
		followBody = nullptr;
	}

	void Application::StopFollowing()
	{
		isTravelling = false;
		followBody = nullptr;
	}

	float Application::GetMinZoom() const
	{
		if (showGalaxyTest)
			return GALAXY_MIN_ZOOM;
		const sRef<CelestrialBody>& body = isTravelling ? travelBody : followBody;
		return body != nullptr ? std::max(MIN_ZOOM, MIN_ZOOM_PER_RADIUS * GetOuterRadius(*body)) : MIN_ZOOM;
	}

	float Application::GetMaxZoom() const
	{
		return showGalaxyTest ? GALAXY_MAX_ZOOM : MAX_ZOOM;
	}

	void Application::UpdateCamera(const float deltaTime)
	{
		// W/A/S/D move the camera's centre while the viewport has focus
		if (mIsViewportFocused && !ImGui::GetIO().WantCaptureKeyboard &&
			(mKeyDown == DH::KeyCode::K_W || mKeyDown == DH::KeyCode::K_A ||
			 mKeyDown == DH::KeyCode::K_S || mKeyDown == DH::KeyCode::K_D))
		{
			StopFollowing();
			camera.UpdateMovement(glm::vec2(0.0f), DH::MouseButton::None, mKeyDown, mKeyMods);
		}

		// Pinch to zoom (consumed even when not over the viewport so it doesn't build up)
		const float magnification = Trackpad::ConsumeMagnification();
		if (magnification != 0.0f && isViewportHovered)
			ZoomBy(magnification * PINCH_ZOOM_SCALE);

		camera.SetPosAcc(PAN_ACCELERATION_PER_ZOOM * std::max(camera.GetZoom(), MIN_ZOOM));

		// Move the render origin to the camera's centre, so the camera's own (float) coordinates stay
		// small wherever it is in the solar system
		cameraOrigin += glm::dvec3(camera.GetCentrePosition());
		camera.SetCentrePosition(glm::vec3(0.0f));

		if (isTravelling)
		{
			// Carried along with the body, then towards it, zooming in proportion. Easing towards a
			// moving body alone would trail it by a distance proportional to its speed (tens of its
			// radii at higher time scales), never arriving.
			const double t = 1.0 - std::pow(1.0 - TRAVEL_RATE, deltaTime * 60.0);
			const glm::dvec3& target = travelBody->GetPosition();
			cameraOrigin += target - travelBodyPosition;
			travelBodyPosition = target;
			cameraOrigin = glm::mix(cameraOrigin, target, t);
			const float zoom = (float)std::exp(glm::mix(std::log((double)camera.GetZoom()), std::log((double)travelZoom), t));

			constexpr double THRESHOLD = 1e-3;
			if (glm::distance(cameraOrigin, target) < THRESHOLD * travelZoom && std::abs(std::log(zoom / travelZoom)) < THRESHOLD)
			{
				isTravelling = false;
				followBody = travelBody;
				cameraOrigin = target;
				camera.SetZoom(travelZoom);
			}
			else
			{
				camera.SetZoom(zoom);
			}
		}
		else
		{
			if (followBody != nullptr)
				cameraOrigin = followBody->GetPosition();
			// Never inside the body being followed or past the solar system (travel eases into its
			// own zoom, so is left alone)
			camera.SetZoom(std::clamp(camera.GetZoom(), GetMinZoom(), GetMaxZoom()));
		}

		camera.UpdateCamera(deltaTime);
	}

	void Application::ClearViewport(DH::CommandList& cmd)
	{
		DH::RenderPassDesc pass;
		pass.target = mViewportFramebuffer.get();
		pass.clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.BeginRenderPass(pass);
		cmd.EndRenderPass();
	}

	void Application::OnRender(DH::CommandList& cmd)
	{
		if (splashScreen != nullptr)
		{
			splashScreen->Update(mDeltaTime);
			if (splashScreen->IsFinished())
			{
				// Its icon may still be in use by the frame in flight
				DH::GPUContext::Get()->WaitIdle();
				splashScreen.reset();
			}
		}

		// Each slow loading step runs in its own frame, while the last frame (showing the splash
		// screen and what it is waiting for) is on screen
		switch (loadStage)
		{
		case LoadStage::Start:
			loadStage = LoadStage::CreateScene;
			ClearViewport(cmd);
			return;
		case LoadStage::CreateScene:
			CreateScene();
			loadStage = LoadStage::Precompute;
			ClearViewport(cmd);
			return;
		case LoadStage::Precompute:
			// PlanetRenderer::Prepare precomputes the atmospheres in this frame
			loadStage = LoadStage::LoadTextures;
			break;
		default:
			break;
		}

		// The first frame's delta covers start up, and the atmosphere precompute makes a long frame;
		// clamp so the camera and simulation don't jump
		const double frameTime = std::min(mDeltaTime, MAX_FRAME_TIME);
		const float deltaTime = (float)frameTime;
		animationTime += ANIMATION_SPEED * frameTime;

		// The simulation first, so the camera follows a body to where it is this frame
		if (simulateGravity)
			solarSystem->Update(frameTime * daysPerSecond * SECONDS_PER_DAY);
		UpdateCamera(deltaTime);
		if (!showGalaxyTest)
			solarSystem->SetRenderOrigin(cameraOrigin);
		UpdateProjectionMatrix();

		const glm::mat4& view = camera.GetViewMatrix();
		viewProjection = projectionMatrix * view;
		FrameUniforms frame;
		frame.viewProjection = viewProjection;
		frame.view = view;
		frame.projection = projectionMatrix;
		frame.cameraPosition = glm::vec4(camera.GetActualPosition(), 1.0f);
		frame.cameraCentre = glm::vec4(camera.GetCentrePosition(), 1.0f);
		frame.sun = glm::vec4(lightSource->GetTranslation(), lightSource->GetScale().x);
		frame.times = glm::vec4((float)animationTime, (float)animationTime * 0.05f, 0.0f, 0.0f);
		frame.display = glm::vec4(exposure, 0.0f, 0.0f, 0.0f);
		if (showGalaxyTest)
		{
			RenderGalaxyTest(cmd, frame);
			return;
		}
		SetOccluders(frame);
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

		if (showGrid)
			RenderGrid(cmd);

		// Draw the furthest kind of body first so blending (coronas, atmospheres) composites correctly
		const float pixelAngle = 2.0f * std::tan(glm::radians(FOV) * 0.5f) / (float)std::max<size_t>(mViewportFramebuffer->GetHeight(), 1);
		auto RenderPlanets = [&]() { planetRenderer->Render(cmd, polygonMode, cameraPosition, pixelAngle); };
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

		// The Sun's glare when it is far away, over everything
		const glm::vec2 viewportSize((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		starRenderer->RenderFlares(cmd, FlareView{ viewProjection, cameraPosition, projectionMatrix[1][1], viewportSize },
								   solarSystem->GetBodies());

		cmd.EndRenderPass();
	}

	void Application::RenderGrid(DH::CommandList& cmd)
	{
		const double level = std::log10(std::max(camera.GetZoom(), MIN_ZOOM) / GRID_SPACINGS_PER_ZOOM);
		const double fineSpacing = std::pow(10.0, std::floor(level));
		const float fineOpacity = GRID_OPACITY * (1.0f - (float)(level - std::floor(level)));

		// Lines on multiples of the spacing in the solar system's coordinates (so they stay put as the
		// camera moves), in the ecliptic (y = 0), around the camera's centre
		const glm::dvec3 centre = cameraOrigin + glm::dvec3(camera.GetCentrePosition());
		cmd.BindPipeline(*gridPipeline);
		for (const auto& [spacing, opacity] : { std::pair{ fineSpacing, fineOpacity }, std::pair{ 10.0 * fineSpacing, GRID_OPACITY } })
		{
			const double halfSize = 0.5 * GRID_LINES * spacing;
			const glm::dvec3 corner(std::floor(centre.x / spacing) * spacing - halfSize, 0.0,
									std::floor(centre.z / spacing) * spacing - halfSize);
			const glm::mat4 transform = glm::translate(glm::vec3(corner - cameraOrigin)) *
										glm::scale(glm::vec3((float)(GRID_LINES * spacing)));
			cmd.PushConstants(GridPushConstants{ transform, glm::vec4((float)(GRID_FADE_SPACINGS * spacing), opacity, 0.0f, 0.0f) });
			gridMesh->Draw(cmd);
		}
	}

	void Application::OnImGuiRender()
	{
		if (applyDefaultLayout)
		{
			ApplyDefaultLayout();
			applyDefaultLayout = false;
		}

		// The scene's interface, once the scene has been rendered
		if (loadStage >= LoadStage::LoadTextures)
		{
			// GraphicsApp drew the viewport window this frame; reopen it to ask whether the cursor
			// is over it, so scrolling and pinching work without clicking the viewport first, and to
			// label the bodies over it
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			if (ImGui::Begin(mViewportName.c_str()))
			{
				// (Not over the popups opened from it, such as a body's menu)
				isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy);
				labelRects.clear();
				if (showGalaxyTest)
				{
					selectedBody.reset();
					if (showLabels)
						DrawGalaxyLabels();
				}
				else
				{
					if (showLabels)
						DrawLabels();
					DrawCrosshair();
					UpdateBodySelection();
				}
			}
			else
			{
				isViewportHovered = false;
			}
			ImGui::End();
			ImGui::PopStyleVar();

			RenderControlsPanel();
		}

		if (splashScreen != nullptr)
			DrawSplashScreen();
	}

	void Application::DrawSplashScreen()
	{
		// Progress through the loading steps, the textures taking the most time
		float progress = 0.0f;
		std::string status;
		switch (loadStage)
		{
		case LoadStage::Start:
		case LoadStage::CreateScene:
			status = "Creating the solar system";
			break;
		case LoadStage::Precompute:
			progress = 0.15f;
			status = "Precomputing atmospheres";
			break;
		case LoadStage::LoadTextures:
		{
			const PlanetRenderer::MapLoadProgress maps = planetRenderer->GetMapLoadProgress();
			const float fraction = maps.total > 0 ? (float)maps.loaded / (float)maps.total : 1.0f;
			progress = 0.4f + 0.6f * fraction;
			status = "Loading textures (" + std::to_string(maps.loaded) + " of " + std::to_string(maps.total) + ")";
			if ((maps.loaded == maps.total && splashScreen->GetAge() >= MIN_SPLASH_SECONDS) ||
				splashScreen->GetAge() >= MAX_SPLASH_SECONDS)
			{
				splashScreen->Dismiss();
				loadStage = LoadStage::Loaded;
			}
			break;
		}
		case LoadStage::Loaded:
			progress = 1.0f;
			status = "Ready";
			break;
		}
		splashScreen->Draw(progress, status);
	}

	void Application::ApplyDefaultLayout()
	{
		// Dahlia's dock space over the main window (the only root dock space)
		ImGuiDockNode* dockSpace = nullptr;
		for (const ImGuiStoragePair& pair : GImGui->DockContext.Nodes.Data)
		{
			auto* node = (ImGuiDockNode*)pair.val_p;
			if (node != nullptr && node->IsDockSpace() && node->IsRootNode())
				dockSpace = node;
		}
		if (dockSpace == nullptr)
			return;

		// The controls panel on the left, the viewport filling the rest
		const ImGuiID id = dockSpace->ID;
		const ImVec2 size = dockSpace->Size;
		ImGui::DockBuilderRemoveNode(id);
		ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(id, size);
		ImGuiID left = 0, right = 0;
		ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, CONTROLS_PANEL_FRACTION, &left, &right);
		ImGui::DockBuilderDockWindow("Space Visualiser", left);
		ImGui::DockBuilderDockWindow(mViewportName.c_str(), right);
		ImGui::DockBuilderFinish(id);
	}

	void Application::DrawLabels()
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 colour = IM_COL32(220, 220, 220, 210);
		const ImU32 moonColour = IM_COL32(180, 190, 205, 190);
		const float fontSize = ImGui::GetFontSize();

		// Each body's disc on screen
		struct Disc
		{
			glm::vec2 centre;
			float radius;
			float depth; // Clip w: the distance in front of the camera
		};
		std::vector<Disc> discs;
		discs.reserve(namedBodies.size());
		for (const NamedBody& namedBody : namedBodies)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(namedBody.body->GetTranslation(), 1.0f);
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			discs.push_back({ origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size,
							  GetOuterRadius(*namedBody.body) / clip.w * projectionMatrix[1][1] * size.y * 0.5f, clip.w });
		}
		// Discs big enough to hide a label behind them
		std::vector<size_t> occluding;
		for (size_t i = 0; i < discs.size(); i++)
		{
			if (discs[i].depth > 0.0f && discs[i].radius >= 1.0f)
				occluding.push_back(i);
		}

		// The Sun and planets first, then the moons from the largest down, so where labels would
		// overlap the more notable body keeps its label
		std::vector<size_t> order(namedBodies.size());
		for (size_t i = 0; i < order.size(); i++)
			order[i] = i;
		std::stable_sort(order.begin(), order.end(), [this](const size_t a, const size_t b)
			{
				const bool aMoon = !namedBodies[a].parent.empty(), bMoon = !namedBodies[b].parent.empty();
				if (aMoon != bMoon)
					return !aMoon;
				return aMoon && namedBodies[a].body->GetScale().x > namedBodies[b].body->GetScale().x;
			});

		std::vector<ImVec4> placed; // Label rectangles drawn (min x, min y, max x, max y)
		for (const size_t i : order)
		{
			const Disc& disc = discs[i];
			const glm::vec2& point = disc.centre;
			const float discRadius = disc.radius;
			if (disc.depth <= 0.0f || discRadius > LABEL_MAX_DISC_FRACTION * size.y)
				continue;
			if (point.x < origin.x - size.x * 0.25f || point.x > origin.x + size.x * 1.25f ||
				point.y < origin.y - size.y * 0.25f || point.y > origin.y + size.y * 1.25f)
				continue;

			// Hidden behind a nearer body's disc
			bool hidden = false;
			for (const size_t other : occluding)
			{
				if (other != i && discs[other].depth < disc.depth && glm::distance(discs[other].centre, point) < discs[other].radius)
				{
					hidden = true;
					break;
				}
			}
			if (hidden)
				continue;

			// Left out where it would overlap a label already drawn
			const NamedBody& namedBody = namedBodies[i];
			const float offset = std::max(discRadius, LABEL_DOT_RADIUS) + 4.0f;
			const ImVec2 textSize = ImGui::CalcTextSize(namedBody.name.c_str());
			const ImVec4 rect(point.x - LABEL_DOT_RADIUS - LABEL_PADDING, point.y - fontSize * 0.5f - LABEL_PADDING,
							  point.x + offset + textSize.x + LABEL_PADDING, point.y + fontSize * 0.5f + LABEL_PADDING);
			bool overlaps = false;
			for (const ImVec4& other : placed)
			{
				if (rect.x < other.z && rect.z > other.x && rect.y < other.w && rect.w > other.y)
				{
					overlaps = true;
					break;
				}
			}
			if (overlaps)
				continue;
			placed.push_back(rect);
			labelRects.push_back({ glm::vec4(rect.x, rect.y, rect.z, rect.w), i });

			const ImU32 labelColour = namedBody.parent.empty() ? colour : moonColour;
			if (discRadius < LABEL_DOT_RADIUS)
				drawList->AddCircleFilled(ImVec2(point.x, point.y), namedBody.parent.empty() ? LABEL_DOT_RADIUS : LABEL_DOT_RADIUS * 0.7f, labelColour);
			drawList->AddText(ImVec2(point.x + offset, point.y - fontSize * 0.5f), labelColour, namedBody.name.c_str());
		}
	}

	std::optional<size_t> Application::PickBody(const glm::vec2 point) const
	{
		// The ray through the point, in render coordinates (in doubles: bodies far from the camera
		// are tiny next to their distances)
		const glm::vec2 ndc(point.x * 2.0f - 1.0f, 1.0f - point.y * 2.0f);
		const glm::dvec3 eye(camera.GetActualPosition());
		const glm::dvec3 direction = glm::normalize(glm::dvec3(glm::mat3(camera.GetInverseViewMatrix()) *
			glm::vec3(ndc.x / projectionMatrix[0][0], ndc.y / projectionMatrix[1][1], -1.0f)));
		// The size of a pixel per unit of distance
		const double pixelAngle = 2.0 / (projectionMatrix[1][1] * (double)mViewportFramebuffer->GetHeight());

		// The first sphere the ray enters, each grown to at least PICK_RADIUS_PIXELS on screen, so a
		// small moon in front of its planet is picked by clicking near it, but not one behind it
		std::optional<size_t> picked;
		double nearest = std::numeric_limits<double>::max();
		for (size_t i = 0; i < namedBodies.size(); i++)
		{
			const CelestrialBody& body = *namedBodies[i].body;
			const glm::dvec3 toCentre = glm::dvec3(body.GetTranslation()) - eye;
			const double distance = glm::length(toCentre);
			const double radius = std::max((double)GetOuterRadius(body), PICK_RADIUS_PIXELS * pixelAngle * distance);
			const double along = glm::dot(toCentre, direction);
			const double missSquared = glm::length2(toCentre - along * direction);
			if (missSquared > radius * radius)
				continue;
			const double halfChord = std::sqrt(radius * radius - missSquared);
			if (along + halfChord <= 0.0)
				continue; // Behind the camera
			const double entry = std::max(along - halfChord, 0.0);
			if (entry < nearest)
			{
				nearest = entry;
				picked = i;
			}
		}
		return picked;
	}

	void Application::UpdateBodySelection()
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		const ImGuiIO& io = ImGui::GetIO();
		auto OverImage = [&](const ImVec2& p)
		{
			return p.x >= origin.x && p.y >= origin.y && p.x < origin.x + size.x && p.y < origin.y + size.y;
		};

		// A click (not a drag, which turns the camera) that starts and ends over the viewport selects
		// the body under the cursor: by its label if one is there, otherwise along the ray. (Popups
		// opened from the viewport, such as the body's menu, count as over it unless excluded.)
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy) && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left) && OverImage(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
			OverImage(io.MousePos) && size.x > 0.0f && size.y > 0.0f)
		{
			const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
			selectedBody.reset();
			for (const auto& [rect, index] : labelRects)
			{
				if (mouse.x >= rect.x && mouse.x <= rect.z && mouse.y >= rect.y && mouse.y <= rect.w)
				{
					selectedBody = index;
					break;
				}
			}
			if (!selectedBody)
				selectedBody = PickBody((mouse - origin) / size);
			if (selectedBody)
				ImGui::OpenPopup(BODY_MENU);
		}

		if (!selectedBody || !ImGui::IsPopupOpen(BODY_MENU))
		{
			selectedBody.reset();
			return;
		}
		const NamedBody& namedBody = namedBodies[*selectedBody];

		// Ring the selected body while its menu is open (a dark outline under a light line)
		const glm::vec4 clip = viewProjection * glm::vec4(namedBody.body->GetTranslation(), 1.0f);
		if (clip.w > 0.0f)
		{
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			const ImVec2 centre(origin.x + (ndc.x * 0.5f + 0.5f) * size.x, origin.y + (0.5f - ndc.y * 0.5f) * size.y);
			const float discRadius = GetOuterRadius(*namedBody.body) / clip.w * projectionMatrix[1][1] * size.y * 0.5f;
			const float ringRadius = std::max(discRadius, LABEL_DOT_RADIUS) + SELECTION_RING_GAP;
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddCircle(centre, ringRadius, IM_COL32(0, 0, 0, 140), 0, 3.5f);
			drawList->AddCircle(centre, ringRadius, IM_COL32(235, 235, 235, 230), 0, 1.5f);
		}

		if (ImGui::BeginPopup(BODY_MENU))
		{
			ImGui::TextUnformatted(namedBody.name.c_str());
			if (!namedBody.parent.empty())
				ImGui::TextDisabled("Moon of %s", namedBody.parent.c_str());
			ImGui::Separator();
			if (ImGui::MenuItem("Go to object"))
				MoveTo(namedBody.body, GO_TO_ZOOM * GetOuterRadius(*namedBody.body));
			ImGui::EndPopup();
		}
	}

	void Application::SetOccluders(FrameUniforms& frame) const
	{
		// By apparent size from the camera's centre
		const glm::vec3 centre = camera.GetCentrePosition();
		std::vector<std::pair<float, const CelestrialBody*>> candidates;
		for (const sRef<CelestrialBody>& body : solarSystem->GetBodies())
		{
			if (body->GetType() == CelestrialBodyType::Star)
				continue;
			const float radius = body->GetScale().x;
			const float distance = std::max(glm::distance(body->GetTranslation(), centre), radius);
			candidates.push_back({ radius / distance, body.get() });
		}
		const size_t count = std::min<size_t>(candidates.size(), MAX_OCCLUDERS);
		std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(),
						  [](const auto& a, const auto& b) { return a.first > b.first; });

		frame.occluderCount = glm::vec4((float)count, 0.0f, 0.0f, 0.0f);
		for (size_t i = 0; i < count; i++)
		{
			const CelestrialBody& body = *candidates[i].second;
			// Earth's atmosphere reddens its umbra (the only one thick enough to light a moon in its shadow)
			const bool reddens = body.GetType() == CelestrialBodyType::Planet && dynamic_cast<const Earth*>(&body) != nullptr;
			frame.occluders[i] = glm::vec4(body.GetTranslation(), reddens ? -body.GetScale().x : body.GetScale().x);
		}
	}

	void Application::DrawCrosshair()
	{
		// The camera looks at its orbit centre, so it is always at the middle of the viewport
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		const glm::vec2 centre = glm::floor(GetViewportImageOrigin() + 0.5f * size) + 0.5f;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// A dark outline under light lines, so it shows over bright bodies as well as space
		for (const auto& [colour, thickness] : { std::pair{ IM_COL32(0, 0, 0, 140), CROSSHAIR_THICKNESS + 2.0f },
												 std::pair{ IM_COL32(255, 255, 255, 220), CROSSHAIR_THICKNESS } })
		{
			for (const glm::vec2 direction : { glm::vec2(1.0f, 0.0f), glm::vec2(-1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, -1.0f) })
			{
				const glm::vec2 from = centre + direction * CROSSHAIR_GAP;
				const glm::vec2 to = centre + direction * (CROSSHAIR_GAP + CROSSHAIR_ARM);
				drawList->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), colour, thickness);
			}
		}
	}

	Application::CameraState Application::GetGalaxyOverview(const GalaxyType type)
	{
		const GalaxyPreset& preset = GetGalaxyPreset(type);
		CameraState state;
		state.camera.SetZoom(preset.overviewZoom);
		TurnCamera(state.camera, preset.overviewPitch, preset.overviewYaw);
		return state;
	}

	void Application::SetGalaxyType(const GalaxyType type)
	{
		galaxyType = type;
		galaxyRenderer->parameters = GetGalaxyPreset(type).parameters;
		galaxyRenderer->Regenerate();
		const CameraState overview = GetGalaxyOverview(type);
		camera = overview.camera;
		cameraOrigin = overview.origin;
		UpdateProjectionMatrix();
	}

	void Application::SetGalaxyTest(const bool enabled)
	{
		if (enabled == showGalaxyTest)
			return;
		if (enabled && galaxyRenderer == nullptr)
			galaxyRenderer = umake<GalaxyRenderer>(sceneContext, GetGalaxyPreset(galaxyType).parameters);

		// Each view keeps its own camera, so switching back returns to where it was
		CameraState shown{ camera, cameraOrigin, followBody, isTravelling, travelBody, travelZoom };
		camera = otherCameraState.camera;
		cameraOrigin = otherCameraState.origin;
		followBody = otherCameraState.followBody;
		isTravelling = otherCameraState.isTravelling;
		travelBody = otherCameraState.travelBody;
		travelZoom = otherCameraState.travelZoom;
		otherCameraState = std::move(shown);

		showGalaxyTest = enabled;
		UpdateProjectionMatrix();
	}

	void Application::RenderGalaxyTest(DH::CommandList& cmd, FrameUniforms& frame)
	{
		// Nothing casts shadows, and the Sun isn't drawn
		frame.occluderCount = glm::vec4(0.0f);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// The solar system's textures keep loading while it is hidden
		planetRenderer->Prepare(cmd);

		const float fieldOfView = glm::radians(GetGalaxyPreset(galaxyType).fieldOfView);
		const float pixelAngle = 2.0f * std::tan(fieldOfView * 0.5f) / (float)std::max<size_t>(mViewportFramebuffer->GetHeight(), 1);
		galaxyRenderer->Render(cmd, *mViewportFramebuffer, GalaxyView{ glm::vec3(-cameraOrigin), pixelAngle });
	}

	void Application::RenderGalaxyControls()
	{
		if (!ImGui::CollapsingHeader("Galaxy", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		GalaxyParameters& p = galaxyRenderer->parameters;
		ImGui::Checkbox("Show labels (L)", &showLabels);
		ImGui::SliderFloat("Brightness", &p.brightness, 0.05f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Star brightness", &p.starBrightness, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Bulge glow", &p.bulgeGlow, 0.0f, 10.0f);
		ImGui::SliderFloat("Disc glow", &p.discGlow, 0.0f, 10.0f);
		ImGui::SliderFloat("Arm glow", &p.armGlow, 0.0f, 5.0f);
		ImGui::SliderFloat("Nebulae", &p.nebulaGlow, 0.0f, 5.0f);
		ImGui::SetItemTooltip("Brightness of the H II regions (glowing hydrogen) along the arms");
		if (!p.companions.empty())
			ImGui::SliderFloat("Companions", &p.companionGlow, 0.0f, 5.0f);
		ImGui::SliderFloat("Dust", &p.dustDepth, 0.0f, 10.0f);
		ImGui::SetItemTooltip("Optical depth of the dust seen face-on through an arm (0 for none)");
		ImGui::SliderFloat("Dust thickness", &p.dustScaleHeight, 20.0f, 500.0f, "%.0f pc");

		// The glow follows these straight away; the stars are placed again when a slider is let go
		ImGui::SeparatorText("Shape");
		bool regenerate = false;
		auto Reshape = [&regenerate]() { regenerate |= ImGui::IsItemDeactivatedAfterEdit(); };
		ImGui::SliderFloat("Bar angle", &p.barAngle, 0.0f, 90.0f, "%.0f deg");
		Reshape();
		ImGui::SetItemTooltip("From the +x axis (in the Milky Way, the line between the Sun and the galactic centre)");
		ImGui::SliderFloat("Bar share", &p.barShare, 0.0f, 1.0f);
		Reshape();
		ImGui::SetItemTooltip("The bar's share of the central light (the rest is in a round bulge)");
		ImGui::SliderFloat("Bar length", &p.barHalfLength, 1000.0f, 8000.0f, "%.0f pc");
		Reshape();
		ImGui::SliderInt("Arms", &p.armCount, 2, 8);
		Reshape();
		ImGui::SliderFloat("Arm pitch", &p.armPitch, 5.0f, 30.0f, "%.1f deg");
		Reshape();
		ImGui::SliderFloat("Arm start", &p.armRadius, 500.0f, 10000.0f, "%.0f pc");
		Reshape();
		ImGui::SetItemTooltip("Where the major arms leave the ends of the bar");
		ImGui::SliderFloat("Minor arms", &p.minorArmStrength, 0.0f, 1.0f);
		Reshape();
		ImGui::SetItemTooltip("The minor arms' young stars, relative to the major arms'");
		ImGui::SliderFloat("Arm width", &p.armWidth, 100.0f, 1500.0f, "%.0f pc");
		Reshape();
		ImGui::SliderFloat("Disc scale length", &p.discScaleLength, 1000.0f, 8000.0f, "%.0f pc");
		Reshape();
		ImGui::SliderFloat("Ring radius", &p.ringRadius, 1000.0f, 20000.0f, "%.0f pc");
		Reshape();
		ImGui::SliderFloat("Ring strength", &p.ringStrength, 0.0f, 3.0f);
		Reshape();
		ImGui::SetItemTooltip("A ring of young stars, nebulae and dust, relative to the arms (0 for none)");
		ImGui::SliderFloat("Disc thickness", &p.discScaleHeight, 50.0f, 1000.0f, "%.0f pc");
		Reshape();
		ImGui::SliderInt("Stars", &p.starCount, 1000, 2000000, "%d", ImGuiSliderFlags_Logarithmic);
		Reshape();

		if (ImGui::Button("Defaults"))
		{
			p = GetGalaxyPreset(galaxyType).parameters;
			regenerate = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%zu stars", galaxyRenderer->GetStarCount());

		if (regenerate)
			galaxyRenderer->Regenerate();
	}

	void Application::DrawGalaxyLabels()
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float fontSize = ImGui::GetFontSize();

		std::vector<GalaxyMarker> labels = GetGalaxyPreset(galaxyType).markers;
		for (const GalaxyCompanion& companion : galaxyRenderer->parameters.companions)
		{
			if (!companion.name.empty())
				labels.push_back({ companion.name, companion.position });
		}
		for (const auto& [name, position] : labels)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(position - glm::vec3(cameraOrigin), 1.0f);
			if (clip.w <= 0.0f)
				continue;
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			const glm::vec2 point = origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size;
			const ImU32 colour = IM_COL32(220, 220, 220, 210);
			drawList->AddCircle(ImVec2(point.x, point.y), LABEL_DOT_RADIUS * 1.6f, colour, 0, 1.5f);
			drawList->AddText(ImVec2(point.x + LABEL_DOT_RADIUS * 1.6f + 4.0f, point.y - fontSize * 0.5f), colour, name.c_str());
		}
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
			bool galaxyTest = showGalaxyTest;
			if (ImGui::Checkbox("Galaxy test", &galaxyTest))
				SetGalaxyTest(galaxyTest);
			ImGui::SetItemTooltip("Show only a galaxy, in place of the solar system");
			if (showGalaxyTest)
			{
				const GalaxyPreset& current = GetGalaxyPreset(galaxyType);
				if (ImGui::BeginCombo("Galaxy", current.name.c_str()))
				{
					for (size_t i = 0; i < (size_t)GalaxyType::Count; i++)
					{
						const GalaxyType type = (GalaxyType)i;
						const bool selected = type == galaxyType;
						if (ImGui::Selectable(GetGalaxyPreset(type).name.c_str(), selected) && !selected)
							SetGalaxyType(type);
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			if (!showGalaxyTest)
			{
				ImGui::DragFloat("Exposure (-/=)", &exposure, 0.1f, 0.0f, 100.0f);
				ImGui::SliderFloat("H-alpha emission", &starRenderer->hydrogenEmission, 0.0f, 4.0f);
				ImGui::SliderFloat("Solar flares", &starRenderer->solarFlares, 0.0f, 2.0f);
				ImGui::SetItemTooltip("Brightness of the flares on the Sun's surface and in the loops over its limb (0 for none)");
				ImGui::SliderFloat("Sun flare", &starRenderer->flareIntensity, 0.0f, 2.0f);
				ImGui::SetItemTooltip("The Sun's glare and lens flare when it is far away (0 for none)");
				ImGui::SliderFloat("Faint ring gain", &planetRenderer->diffuseRingGain, 0.0f, 3000.0f, "%.0f",
								   ImGuiSliderFlags_Logarithmic);

				int mode = (int)polygonMode;
				const char* modes[] = { "Solid (1)", "Wireframe (2)", "Points (3)" };
				if (ImGui::Combo("Render style", &mode, modes, 3))
					polygonMode = (DH::PolygonMode)mode;

				ImGui::Checkbox("Show grid (G)", &showGrid);
				ImGui::SameLine();
				ImGui::Checkbox("Show labels (L)", &showLabels);
				ImGui::Checkbox("Simulate gravity", &simulateGravity);
				ImGui::SameLine();
				ImGui::TextDisabled("%s", FormatDate(solarSystem->GetTime()).c_str());
				ImGui::SliderFloat("Time scale", &daysPerSecond, 0.001f, 100.0f, "%.3g days/s", ImGuiSliderFlags_Logarithmic);
			}
		}

		if (showGalaxyTest)
			RenderGalaxyControls();

		if (!showGalaxyTest && ImGui::CollapsingHeader("Bodies", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto GoToButton = [this](const NamedBody& namedBody)
			{
				if (namedBody.parent.empty() ? ImGui::Button("Go to") : ImGui::SmallButton("Go to"))
					MoveTo(namedBody.body, GO_TO_ZOOM * GetOuterRadius(*namedBody.body));
				ImGui::SameLine();
			};

			// Each planet, then its moons (in order from the planet) under it
			for (size_t i = 0; i < namedBodies.size(); i++)
			{
				const NamedBody& namedBody = namedBodies[i];
				if (!namedBody.parent.empty())
					continue;
				ImGui::PushID(namedBody.name.c_str());
				GoToButton(namedBody);
				if (namedBody.body == lightSource)
				{
					ImGui::TextUnformatted(namedBody.name.c_str());
				}
				else
				{
					const double distance = glm::distance(namedBody.body->GetPosition(), lightSource->GetPosition());
					ImGui::Text("%s (%.2f AU from the Sun)", namedBody.name.c_str(), distance / Universe::AU_IN_UNITS);
				}

				size_t moonCount = 0;
				for (size_t j = i + 1; j < namedBodies.size() && namedBodies[j].parent == namedBody.name; j++)
					moonCount++;
				if (moonCount > 0)
				{
					ImGui::Indent();
					const std::string header = moonCount == 1 ? "1 moon" : std::to_string(moonCount) + " moons";
					if (ImGui::TreeNode("Moons", "%s", header.c_str()))
					{
						for (size_t j = i + 1; j <= i + moonCount; j++)
						{
							const NamedBody& moonBody = namedBodies[j];
							const auto& moon = static_cast<const Moon&>(*moonBody.body);
							const Universe::MoonData& data = moon.GetData();
							ImGui::PushID((int)j);
							GoToButton(moonBody);
							const double distance = glm::distance(moonBody.body->GetPosition(), namedBody.body->GetPosition()) *
													Universe::KM_PER_UNIT;
							ImGui::Text("%s (%s%.0f km across, %.0f km from %s)", moonBody.name.c_str(), data.radiusEstimated ? "~" : "",
										2.0 * data.radiusKm, distance, namedBody.name.c_str());
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip("%s%s%s", data.regular ? "Regular moon" : "Irregular moon (a captured body on a distant, inclined orbit)",
												  data.radiusEstimated ? "\nSize estimated from its brightness" : "",
												  data.fitError > 1.0 ? "\nPosition approximate (its orbit is strongly perturbed)" : "");
							}
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
					ImGui::Unindent();
				}
				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const glm::dvec3 centre = cameraOrigin + glm::dvec3(camera.GetCentrePosition());
			if (showGalaxyTest)
			{
				const glm::dvec3 kpc = centre / 1000.0;
				ImGui::Text("Centre (kpc): X=%.3f, Y=%.3f, Z=%.3f", kpc.x, kpc.y, kpc.z);
				ImGui::Text("Zoom: %.3g kpc", camera.GetZoom() / 1000.0f);
			}
			else
			{
				const glm::dvec3 au = centre / Universe::AU_IN_UNITS;
				ImGui::Text("Centre (AU): X=%.4f, Y=%.4f, Z=%.4f", au.x, au.y, au.z);
				ImGui::Text("Zoom: %.3g Earth radii (%.3g AU)", camera.GetZoom(), camera.GetZoom() / Universe::AU_IN_UNITS);
			}
			ImGui::Text("Pitch: %.1f, Yaw: %.1f", camera.GetPitch(), camera.GetYaw());
			for (const NamedBody& namedBody : namedBodies)
			{
				if (namedBody.body == followBody)
					ImGui::Text("Following %s", namedBody.name.c_str());
			}
			if (ImGui::Button("Reset camera"))
			{
				if (showGalaxyTest)
				{
					const CameraState overview = GetGalaxyOverview(galaxyType);
					camera = overview.camera;
					cameraOrigin = overview.origin;
				}
				else
				{
					MoveTo(lightSource, (float)OVERVIEW_ZOOM);
				}
			}
			ImGui::TextWrapped("Trackpad: two finger scroll to orbit, pinch or Cmd + scroll to zoom, Shift + scroll to pan.");
			ImGui::TextWrapped("Mouse: drag to orbit, right drag or Option + drag to pan, wheel or Shift + drag to zoom.");
			ImGui::TextWrapped("W/A/S/D: move. Panning or moving stops following a body.");
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
		case DH::KeyCode::K_G:
			showGrid = !showGrid;
			break;
		case DH::KeyCode::K_L:
			showLabels = !showLabels;
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
			const ImGuiIO& io = ImGui::GetIO();
			const glm::vec2 delta = mDeltaMousePos / glm::vec2((float)mWindow->GetWidth(), (float)mWindow->GetHeight());

			// Shift + drag zooms (dragging down zooms out). Camera3D's own Shift zoom moves the
			// distance at a fixed speed with momentum, which crawls across the solar system, leaps
			// past bodies up close and can zoom through them, so the zoom is applied here instead:
			// exponentially (like scrolling) and within the zoom limits.
			if (io.KeyShift)
			{
				ZoomBy(-delta.y * DRAG_ZOOM_SCALE);
				return true;
			}

			// Option + drag pans, for trackpads where a right drag is awkward
			DH::MouseButton button = mMouseButtonDown;
			if (button == DH::MouseButton::Left && io.KeyAlt)
				button = DH::MouseButton::Right;
			// Panning moves the centre away from the body being followed (orbiting doesn't)
			if (button == DH::MouseButton::Right)
				StopFollowing();

			camera.UpdateMovement(delta, button);
		}
		return true;
	}

	void Application::ZoomBy(const float amount)
	{
		// Exponential, so each step covers the same fraction of the distance at any zoom
		const float factor = std::exp(-amount);
		if (isTravelling)
			travelZoom = std::clamp(travelZoom * factor, GetMinZoom(), GetMaxZoom());
		camera.SetZoom(std::clamp(camera.GetZoom() * factor, GetMinZoom(), GetMaxZoom()));
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
		const glm::vec2 delta = offset * 10.0f / glm::vec2((float)mWindow->GetWidth(), (float)mWindow->GetHeight());
		if (io.KeySuper || io.KeyCtrl)
			ZoomBy(offset.y * TRACKPAD_ZOOM_STEP);
		else if (io.KeyShift)
		{
			StopFollowing();
			camera.UpdateMovement(delta, DH::MouseButton::Right);
		}
		else
			camera.UpdateMovement(delta * TRACKPAD_ORBIT_SCALE, DH::MouseButton::Left);
		return true;
	}
}
