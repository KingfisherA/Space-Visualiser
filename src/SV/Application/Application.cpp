#include "Application.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
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
#include "SV/Universe/CompactObject.hpp"
#include "SV/Universe/Galaxy.hpp"
#include "SV/Universe/MoonData.hpp"
#include "SV/Universe/Nebula.hpp"
#include "SV/Universe/PlanetarySystem.hpp"
#include "SV/Universe/SolarSystem.hpp"
#include "SV/Universe/StarSystem.hpp"

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
		// Out past the most distant galaxies of the catalogues (1.5 Gpc)
		constexpr float MAX_ZOOM = (float)(1.5e9 * PARSEC_IN_UNITS);
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
		// W/A/S/D move the camera's centre this many zooms (its distance from the centre), or distances to
		// the nearest surface if that is nearer, per second
		constexpr float KEY_MOVE_SPEED = 1.0f;
		// "Go to" zoom distance per unit of the body's radius
		constexpr float GO_TO_ZOOM = 6.0f;
		// Fraction of the remaining distance (and log zoom) covered each 1/60 s when travelling to a body
		constexpr float TRAVEL_RATE = 0.11f;
		// "Reset camera" zoom: the inner solar system, around the Sun
		constexpr double OVERVIEW_ZOOM = 4.0 * Universe::AU_IN_UNITS;
		// Travelling further than this (AU) zooms out on the way, to this fraction of the distance
		// left, so the destination stays in view rather than rushing past
		constexpr double FAR_TRAVEL = 1000.0;
		constexpr double FAR_TRAVEL_ZOOM = 0.6;
		constexpr double FAR_TRAVEL_RATE = 0.045; // As TRAVEL_RATE

		// The universe: the solar system is drawn while the camera is within SOLAR_SYSTEM_EXTENT of
		// the Sun (AU), and the Sun drawn as a star of the universe from SUN_POINT_START (its glare
		// fading out as it does)
		constexpr double SUN_POINT_START = 2000.0;
		constexpr double SOLAR_SYSTEM_EXTENT = 20000.0;
		// Universe landmarks: at most this many labels (see UniverseRenderer::GetProminence for which),
		// or this many inside the solar system (around its own labels)
		constexpr size_t MAX_LANDMARK_LABELS = 60;
		constexpr size_t MAX_SKY_LABELS = 20;
		// Clicking the sky picks a landmark within this many pixels
		constexpr float LANDMARK_PICK_PIXELS = 10.0f;
		// Landmarks near the cursor are picked by their prominence (up to this) less twice their
		// distance from it in LANDMARK_PICK_PIXELS
		constexpr float LANDMARK_PICK_MAX_PROMINENCE = 4.0f;
		constexpr const char* LANDMARK_MENU = "LandmarkMenu";
		// "Go to" a landmark: the zoom (in its radii, or parsecs for points)
		constexpr double LANDMARK_ZOOM_RADII = 3.0;
		constexpr double LANDMARK_POINT_ZOOM = 2.0;
		constexpr size_t MAX_SEARCH_RESULTS = 200;

		// The galaxy test works in parsecs: zoom limits, the near plane as a fraction of the zoom,
		// and how far past the galactic centre anything is drawn (each galaxy's first view is in its
		// GalaxyPreset)
		constexpr float GALAXY_MIN_ZOOM = 20.0f;
		constexpr float GALAXY_MAX_ZOOM = 500000.0f;
		constexpr float GALAXY_NEAR_PLANE_FRACTION = 1e-3f;
		constexpr float GALAXY_EXTENT = 60000.0f;

		// The black hole and pulsar tests work in Schwarzschild radii: zoom limits (outside the
		// horizon or the star), the field of view (degrees) and the first views (just above the
		// disc: for the black hole, as in Interstellar)
		constexpr float BLACK_HOLE_MIN_ZOOM = 2.0f;
		constexpr float PULSAR_MIN_ZOOM = 4.0f;
		constexpr float COMPACT_OBJECT_MAX_ZOOM = 1000.0f;
		constexpr float COMPACT_OBJECT_FOV = 50.0f;
		constexpr float BLACK_HOLE_OVERVIEW_ZOOM = 22.0f;
		constexpr float BLACK_HOLE_OVERVIEW_PITCH = 3.0f;
		constexpr float PULSAR_OVERVIEW_ZOOM = 45.0f;
		constexpr float PULSAR_OVERVIEW_PITCH = 12.0f;

		// The nebula test works in parsecs (each nebula's first view is in its NebulaPreset)
		constexpr float NEBULA_MIN_ZOOM = 0.2f;
		constexpr float NEBULA_MAX_ZOOM = 300.0f;

		// The star test works in solar radii: zoom limits (outside the body followed, at least
		// STAR_MIN_ZOOM_PER_RADIUS of its radius from its centre), and the field of view (degrees)
		constexpr float STAR_MIN_ZOOM = 0.002f;
		constexpr float STAR_MIN_ZOOM_PER_RADIUS = 1.15f;
		constexpr float STAR_MAX_ZOOM = 2.0e6f;
		constexpr float STAR_FOV = 45.0f;
		constexpr double SOLAR_RADII_PER_AU = 215.032;

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
			if (namedBody.name == (std::getenv("SV_START") ? std::getenv("SV_START") : "Earth")) // DEBUGHOOK
			{
				followBody = namedBody.body;
				cameraOrigin = followBody->GetPosition();
				camera.SetZoom((std::getenv("SV_ZOOM") ? (float)std::atof(std::getenv("SV_ZOOM")) : GO_TO_ZOOM) * followBody->GetScale().x);
				if (std::getenv("SV_TURN")) { float p = 0, y = 0; std::sscanf(std::getenv("SV_TURN"), "%f,%f", &p, &y); TurnCamera(camera, p, y); }
			}
		}
		solarSystem->SetRenderOrigin(cameraOrigin);
		camera.UpdateCamera(0.0f);
		UpdateProjectionMatrix();

		// The catalogues (stars, galaxies, nebulae and pulsars) load in the background
		pendingCatalogue = std::async(std::launch::async, []() { return LoadCatalogue(GetCatalogueDirectory()); });
	}

	void Application::OnDestroy()
	{
		Trackpad::Uninstall();

		// GPU resources must be released before Dahlia destroys the device
		DH::GPUContext::Get()->WaitIdle();
		splashScreen.reset();
		if (pendingCatalogue.valid())
			pendingCatalogue.wait();
		universeRenderer.reset();
		catalogue.reset();
		galaxyRenderer.reset();
		blackHoleRenderer.reset();
		pulsarRenderer.reset();
		nebulaRenderer.reset();
		starSystemRenderer.reset();
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
		if (sceneMode == SceneMode::BlackHole || sceneMode == SceneMode::Pulsar || sceneMode == SceneMode::Nebula || sceneMode == SceneMode::Stars)
		{
			// Only the frame's camera is used (each pixel's ray is traced or marched), so the planes are nominal
			const float fieldOfView = sceneMode == SceneMode::Nebula ? GetNebulaPreset(nebulaType).fieldOfView
									  : sceneMode == SceneMode::Stars ? STAR_FOV
																	  : COMPACT_OBJECT_FOV;
			projectionMatrix = glm::perspective(glm::radians(fieldOfView), width / height, MIN_NEAR_PLANE, 10.0f * GetMaxZoom());
			return;
		}
		if (sceneMode == SceneMode::Galaxy)
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
		camera.StopKeyMovement();
		travelBody = body;
		travelBodyPosition = body->GetPosition();
		travelZoom = zoom;
		isTravelling = true;
		followBody = nullptr;
	}

	void Application::MoveTo(const glm::dvec3& point, const float zoom)
	{
		camera.StopKeyMovement();
		travelBody = nullptr;
		travelPoint = point;
		travelBodyPosition = point;
		travelZoom = zoom;
		isTravelling = true;
		followBody = nullptr;
	}

	void Application::StopFollowing()
	{
		isTravelling = false;
		followBody = nullptr;
		starFocus.reset();
	}

	float Application::GetMinZoom() const
	{
		if (sceneMode == SceneMode::Galaxy)
			return GALAXY_MIN_ZOOM;
		if (sceneMode == SceneMode::BlackHole)
			return BLACK_HOLE_MIN_ZOOM;
		if (sceneMode == SceneMode::Pulsar)
			return PULSAR_MIN_ZOOM;
		if (sceneMode == SceneMode::Nebula)
			return NEBULA_MIN_ZOOM;
		if (sceneMode == SceneMode::Stars)
		{
			const float radius = starFocus.has_value() ? starSystemRenderer->GetNodeRadius(*starFocus) : 0.0f;
			return std::max(STAR_MIN_ZOOM, STAR_MIN_ZOOM_PER_RADIUS * radius);
		}
		const sRef<CelestrialBody>& body = isTravelling ? travelBody : followBody;
		float minZoom = body != nullptr ? std::max(MIN_ZOOM, MIN_ZOOM_PER_RADIUS * GetOuterRadius(*body)) : MIN_ZOOM;
		// Outside a black hole's horizon (or a neutron star) the camera is centred on
		if (showUniverse && universeRenderer != nullptr)
		{
			const glm::dvec3 centre = (cameraOrigin + glm::dvec3(camera.GetCentrePosition()) - lightSource->GetPosition()) / PARSEC_IN_UNITS;
			minZoom = std::max(minZoom, (float)(universeRenderer->GetMinZoom(centre) * PARSEC_IN_UNITS));
		}
		return minZoom;
	}

	float Application::GetMaxZoom() const
	{
		switch (sceneMode)
		{
		case SceneMode::Galaxy:
			return GALAXY_MAX_ZOOM;
		case SceneMode::BlackHole:
		case SceneMode::Pulsar:
			return COMPACT_OBJECT_MAX_ZOOM;
		case SceneMode::Nebula:
			return NEBULA_MAX_ZOOM;
		case SceneMode::Stars:
			return STAR_MAX_ZOOM;
		default:
			return MAX_ZOOM;
		}
	}

	void Application::UpdateCamera(const float deltaTime)
	{
		// W/A/S/D move the camera's centre relative to where it is looking (Camera3D::GetKeyDirection:
		// forwards and backwards along its view, and to its left and right), any number held together
		// (GraphicsApp::IsKeyHeld), at a speed in proportion to the zoom so they work at any scale. The
		// camera eases up to speed and back to rest (Camera3D::UpdateKeyMovement, called every frame so
		// it can slow down after the keys are released). They work while the viewport is focused or
		// under the cursor, and no text is being typed (ImGui's WantCaptureKeyboard is set whenever any
		// of its windows is focused, the viewport included, as keyboard navigation is enabled).
		const ImGuiIO& io = ImGui::GetIO();
		glm::vec3 keyDirection(0.0f);
		if ((mIsViewportFocused || isViewportHovered) && !io.WantTextInput)
			keyDirection = camera.GetKeyDirection(IsKeyHeld(DH::KeyCode::K_W), IsKeyHeld(DH::KeyCode::K_S), IsKeyHeld(DH::KeyCode::K_A),
												  IsKeyHeld(DH::KeyCode::K_D));
		float keySpeed = 0.0f;
		if (glm::length(keyDirection) > 1e-6f)
		{
			StopFollowing();
			// No faster than the distance to the nearest body's surface allows, so the camera slows as
			// it nears one (to see its surface close up) rather than flying through it
			float distance = camera.GetZoom();
			if (sceneMode == SceneMode::SolarSystem && solarSystem != nullptr)
			{
				for (const sRef<CelestrialBody>& body : solarSystem->GetBodies())
				{
					const float surface = glm::distance(body->GetTranslation(), camera.GetActualPosition()) - body->GetScale().x;
					if (surface > 0.0f)
						distance = std::min(distance, surface);
				}
			}
			keySpeed = KEY_MOVE_SPEED * std::max(distance, MIN_ZOOM);
		}
		camera.UpdateKeyMovement(keyDirection, keySpeed, deltaTime);

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
			const glm::dvec3 target = travelBody != nullptr ? travelBody->GetPosition() : travelPoint;
			const bool far = glm::distance(cameraOrigin, target) > FAR_TRAVEL * Universe::AU_IN_UNITS;
			const double t = 1.0 - std::pow(1.0 - (far ? FAR_TRAVEL_RATE : TRAVEL_RATE), deltaTime * 60.0);
			cameraOrigin += target - travelBodyPosition;
			travelBodyPosition = target;
			// Far journeys zoom out to keep the destination in view, moving only as fast as the zoom
			// allows, then zoom in to it
			const double remaining = glm::distance(cameraOrigin, target);
			double goalZoom = travelZoom, move = t;
			if (far)
			{
				goalZoom = std::max(goalZoom, FAR_TRAVEL_ZOOM * remaining);
				move = t * std::clamp((double)camera.GetZoom() / (FAR_TRAVEL_ZOOM * remaining), 0.0, 1.0);
			}
			cameraOrigin = glm::mix(cameraOrigin, target, move);
			const float zoom = (float)std::exp(glm::mix(std::log((double)camera.GetZoom()), std::log(goalZoom), t));

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
			if (sceneMode == SceneMode::Stars && starFocus.has_value())
				cameraOrigin = starSystemRenderer->GetNodePosition(*starFocus);
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
		if (sceneMode == SceneMode::Stars)
			starSystemRenderer->Update(frameTime);
		UpdateCamera(deltaTime);
		if (sceneMode == SceneMode::SolarSystem)
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
		if (sceneMode == SceneMode::Galaxy)
		{
			RenderGalaxyTest(cmd, frame);
			return;
		}
		if (sceneMode == SceneMode::BlackHole || sceneMode == SceneMode::Pulsar)
		{
			RenderCompactObjectTest(cmd, frame, frameTime);
			return;
		}
		if (sceneMode == SceneMode::Nebula)
		{
			RenderNebulaTest(cmd, frame, frameTime);
			return;
		}
		if (sceneMode == SceneMode::Stars)
		{
			RenderStarTest(cmd, frame);
			return;
		}
		SetOccluders(frame);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// Atmosphere precomputation and texture uploads happen outside the scene pass
		planetRenderer->Prepare(cmd);

		// The universe behind the solar system (once its catalogues have loaded)
		if (catalogue == nullptr && pendingCatalogue.valid() &&
			pendingCatalogue.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			catalogue = umake<Catalogue>(pendingCatalogue.get());
			for (const std::string& line : catalogue->report)
				std::cout << "Catalogue: " << line << '\n';
			universeRenderer = umake<UniverseRenderer>(sceneContext, *catalogue);
			universeRenderer->starRenderer = starRenderer;
			// Only the landmarks are kept: the rest is on the GPU
			catalogue->stars = {};
			catalogue->compactSources = {};
			catalogue->nebulae = {};
			catalogue->galaxies = {};
		}
		const bool drawUniverse = showUniverse && universeRenderer != nullptr;
		if (drawUniverse)
			RenderUniverse(cmd, frameTime);
		if (!IsSolarSystemShown())
		{
			if (!drawUniverse)
				ClearViewport(cmd);
			return;
		}

		const glm::vec3& cameraPosition = camera.GetActualPosition();
		planetRenderer->PopulateDistanceCache(cameraPosition);
		starRenderer->PopulateDistanceCache(cameraPosition);

		DH::RenderPassDesc pass;
		pass.target = mViewportFramebuffer.get();
		pass.clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
		// Over the universe
		pass.colourLoadOp = drawUniverse ? DH::LoadOp::Load : DH::LoadOp::Clear;
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

		// The Sun's glare when it is far away, over everything (fading out as the universe draws the
		// Sun as one of its stars)
		const glm::vec2 viewportSize((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		const float flareIntensity = starRenderer->flareIntensity;
		if (drawUniverse)
			starRenderer->flareIntensity *= 1.0f - universeView.sunVisibility;
		starRenderer->RenderFlares(cmd, FlareView{ viewProjection, cameraPosition, projectionMatrix[1][1], viewportSize },
								   solarSystem->GetBodies());
		starRenderer->flareIntensity = flareIntensity;

		cmd.EndRenderPass();
	}

	void Application::RenderGrid(DH::CommandList& cmd)
	{
		const double level = std::log10(std::max(camera.GetZoom(), MIN_ZOOM) / GRID_SPACINGS_PER_ZOOM);
		const double fineSpacing = std::pow(10.0, std::floor(level));
		// Fading out as the camera leaves the solar system (where the universe takes over)
		const double sunDistance = GetSunDistance() / Universe::AU_IN_UNITS;
		const double away = std::clamp((sunDistance - SUN_POINT_START) / (SOLAR_SYSTEM_EXTENT - SUN_POINT_START), 0.0, 1.0);
		const float gridOpacity = GRID_OPACITY * (float)(1.0 - away * away * (3.0 - 2.0 * away));
		const float fineOpacity = gridOpacity * (1.0f - (float)(level - std::floor(level)));

		// Lines on multiples of the spacing in the solar system's coordinates (so they stay put as the
		// camera moves), in the ecliptic (y = 0), around the camera's centre
		const glm::dvec3 centre = cameraOrigin + glm::dvec3(camera.GetCentrePosition());
		cmd.BindPipeline(*gridPipeline);
		for (const auto& [spacing, opacity] : { std::pair{ fineSpacing, fineOpacity }, std::pair{ 10.0 * fineSpacing, gridOpacity } })
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
				if (sceneMode != SceneMode::SolarSystem)
				{
					selectedBody.reset();
					if (sceneMode == SceneMode::Galaxy && showLabels)
						DrawGalaxyLabels();
					if (sceneMode == SceneMode::Nebula && showLabels)
						DrawNebulaLabels();
					if (sceneMode == SceneMode::Stars && showLabels)
						DrawStarLabels();
				}
				else
				{
					if (showLabels && IsSolarSystemShown())
						DrawLabels();
					landmarkLabelRects.clear();
					if (showLabels && showUniverse && universeRenderer != nullptr)
						DrawUniverseLabels();
					DrawCrosshair();
					// (A click on a landmark's label doesn't select a body behind it)
					if (!UpdateLandmarkSelection() && IsSolarSystemShown())
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

	void Application::SetSceneMode(const SceneMode mode)
	{
		if (mode == sceneMode)
			return;
		if (mode == SceneMode::Galaxy && galaxyRenderer == nullptr)
			galaxyRenderer = umake<GalaxyRenderer>(sceneContext, GetGalaxyPreset(galaxyType).parameters);
		if (mode == SceneMode::BlackHole && blackHoleRenderer == nullptr)
			blackHoleRenderer = umake<CompactObjectRenderer>(sceneContext, GetBlackHolePreset());
		if (mode == SceneMode::Pulsar && pulsarRenderer == nullptr)
			pulsarRenderer = umake<CompactObjectRenderer>(sceneContext, GetPulsarPreset());
		if (mode == SceneMode::Nebula && nebulaRenderer == nullptr)
			nebulaRenderer = umake<NebulaRenderer>(sceneContext, GetNebulaPreset(nebulaType).parameters);
		if (mode == SceneMode::Stars && starSystemRenderer == nullptr)
		{
			starSystemRenderer = umake<StarSystemRenderer>(sceneContext, GetStarSystemPreset(starSystemType));
			starFocus = (size_t)GetStarSystemPreset(starSystemType).focus;
		}

		// Each scene keeps its own camera, so switching back returns to where it was
		sceneCameras[(size_t)sceneMode] = CameraState{ camera, cameraOrigin, followBody, isTravelling, travelBody, travelZoom };
		const CameraState& shown = sceneCameras[(size_t)mode];
		camera = shown.camera;
		cameraOrigin = shown.origin;
		followBody = shown.followBody;
		isTravelling = shown.isTravelling;
		travelBody = shown.travelBody;
		travelZoom = shown.travelZoom;

		sceneMode = mode;
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

	Application::CameraState Application::GetCompactObjectOverview(const SceneMode mode)
	{
		const bool isPulsar = mode == SceneMode::Pulsar;
		CameraState state;
		state.camera.SetZoom(isPulsar ? PULSAR_OVERVIEW_ZOOM : BLACK_HOLE_OVERVIEW_ZOOM);
		TurnCamera(state.camera, isPulsar ? PULSAR_OVERVIEW_PITCH : BLACK_HOLE_OVERVIEW_PITCH, 0.0f);
		return state;
	}

	CompactObjectRenderer& Application::GetCompactObjectRenderer() const
	{
		return sceneMode == SceneMode::Pulsar ? *pulsarRenderer : *blackHoleRenderer;
	}

	float Application::GetPixelAngle(const float fieldOfView) const
	{
		return 2.0f * std::tan(glm::radians(fieldOfView) * 0.5f) / (float)std::max<size_t>(mViewportFramebuffer->GetHeight(), 1);
	}

	Application::CameraState Application::GetTestOverview() const
	{
		switch (sceneMode)
		{
		case SceneMode::Galaxy:
			return GetGalaxyOverview(galaxyType);
		case SceneMode::Nebula:
			return GetNebulaOverview(nebulaType);
		case SceneMode::Stars:
			return GetStarSystemOverview(starSystemType);
		default:
			return GetCompactObjectOverview(sceneMode);
		}
	}

	void Application::RenderCompactObjectTest(DH::CommandList& cmd, FrameUniforms& frame, const double deltaTime)
	{
		// Nothing casts shadows, and the Sun isn't drawn
		frame.occluderCount = glm::vec4(0.0f);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// The solar system's textures keep loading while it is hidden
		planetRenderer->Prepare(cmd);

		CompactObjectRenderer& renderer = GetCompactObjectRenderer();
		renderer.Update(deltaTime);
		renderer.Render(cmd, *mViewportFramebuffer, CompactObjectView{ glm::vec3(-cameraOrigin), GetPixelAngle(COMPACT_OBJECT_FOV) });
	}

	void Application::RenderCompactObjectControls()
	{
		CompactObjectParameters& p = GetCompactObjectRenderer().parameters;
		const bool isPulsar = p.type == CompactObjectType::NeutronStar;
		if (!ImGui::CollapsingHeader(isPulsar ? "Pulsar" : "Black hole", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ImGui::SliderFloat("Brightness", &p.brightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Bloom", &p.bloom, 0.0f, 2.0f);
		ImGui::SetItemTooltip("The glow around the brightest light, as scattered in a camera's lens");
		ImGui::SliderFloat("Time scale", &p.timeScale, 0.0f, 50.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
		ImGui::SetItemTooltip("How fast the disc turns and the jets flow (the time light takes to cross a Schwarzschild radius, per second)");
		ImGui::SliderFloat("Gravitational redshift", &p.gravitationalRedshift, 0.0f, 1.0f);
		ImGui::SetItemTooltip("Light from close to the %s loses energy climbing out of its gravity (0 for none)",
							  isPulsar ? "neutron star" : "black hole");

		if (isPulsar)
		{
			ImGui::SeparatorText("Neutron star");
			ImGui::SliderFloat("Spin period", &p.spinPeriod, 0.1f, 10.0f, "%.2f s", ImGuiSliderFlags_Logarithmic);
			ImGui::SetItemTooltip("Real seconds per turn (real pulsars turn in milliseconds to seconds)");
			ImGui::SliderFloat("Magnetic tilt", &p.magneticTilt, 0.0f, 90.0f, "%.0f deg");
			ImGui::SetItemTooltip("Between the spin and magnetic axes: the beams sweep round a cone this wide, and the pulsar flashes as one passes the camera");
			ImGui::SliderFloat("Star brightness", &p.starBrightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Hot spots", &p.hotSpotBrightness, 0.0f, 30.0f, "%.1f");
			ImGui::SetItemTooltip("The magnetic poles, where gas falling along the field lines lands");
			ImGui::SliderFloat("Hot spot size", &p.hotSpotSize, 2.0f, 40.0f, "%.0f deg");
			ImGui::SliderFloat("Beam brightness", &p.beamBrightness, 0.0f, 10.0f, "%.2f");
			ImGui::SetItemTooltip("The beams of radiation from the magnetic poles (radio to X-rays, in false colour)");
			ImGui::SliderFloat("Beam width", &p.beamWidth, 1.0f, 30.0f, "%.1f deg");
			ImGui::SliderFloat("Beam length", &p.beamLength, 5.0f, 200.0f, "%.0f r_s");
			ImGui::SliderFloat("Magnetosphere", &p.magnetosphere, 0.0f, 3.0f, "%.2f");
			ImGui::SetItemTooltip("Plasma glowing along the closed magnetic field lines (0 for none)");
			ImGui::SliderFloat("Star radius", &p.starRadius, 1.6f, 6.0f, "%.2f r_s");
			ImGui::SetItemTooltip("12 km for 1.4 solar masses is 2.9 Schwarzschild radii. Light bends round it, so more than half of it is seen.");
		}

		ImGui::SeparatorText("Accretion disc");
		ImGui::Checkbox("Show disc", &p.showDisc);
		ImGui::BeginDisabled(!p.showDisc);
		ImGui::SliderFloat("Doppler beaming", &p.dopplerBeaming, 0.0f, 1.0f);
		ImGui::SetItemTooltip("The side of the disc turning towards the camera is brighter and bluer, the other side dimmer and redder.\n"
							  "0 shows the disc as if it weren't moving (as Interstellar did), 1 as it would be seen.");
		ImGui::SliderFloat("Temperature", &p.discTemperature, 2000.0f, 30000.0f, "%.0f K", ImGuiSliderFlags_Logarithmic);
		ImGui::SetItemTooltip("The disc's hottest gas, just outside its inner edge, as it is emitted");
		ImGui::SliderFloat("Disc brightness", &p.discBrightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Opacity", &p.discOpacity, 0.0f, 1.0f);
		ImGui::SliderFloat("Turbulence", &p.discTurbulence, 0.0f, 1.0f);
		ImGui::SliderFloat("Inner radius", &p.discInnerRadius, 1.5f, 20.0f, "%.2f r_s");
		ImGui::SetItemTooltip(isPulsar ? "Where the star's magnetic field takes over from the disc" :
										 "3 Schwarzschild radii is the innermost stable orbit, inside which gas falls in");
		ImGui::SliderFloat("Outer radius", &p.discOuterRadius, 4.0f, 60.0f, "%.1f r_s");
		ImGui::EndDisabled();

		ImGui::SeparatorText("Jets");
		ImGui::Checkbox("Show jets", &p.showJets);
		ImGui::SetItemTooltip("Relativistic jets from the poles of the spin axis, glowing in X-rays (shown in false colour)");
		ImGui::BeginDisabled(!p.showJets);
		ImGui::SliderFloat("Jet brightness", &p.jetBrightness, 0.01f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Jet speed", &p.jetSpeed, 0.0f, 0.99f, "%.2f c");
		ImGui::SetItemTooltip("The jets' gas's speed: the faster, the brighter the jet coming towards the camera and the fainter the other");
		ImGui::SliderFloat("Opening angle", &p.jetOpeningAngle, 0.5f, 15.0f, "%.1f deg");
		ImGui::SliderFloat("Jet length", &p.jetLength, 10.0f, 200.0f, "%.0f r_s");
		ImGui::EndDisabled();

		ImGui::SeparatorText("Sky");
		ImGui::SliderFloat("Stars", &p.starFieldBrightness, 0.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Sky glow", &p.skyBrightness, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Resolution", &p.renderScale, 0.25f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("The fraction of the viewport's resolution the light rays are traced at");

		if (ImGui::Button("Defaults"))
			p = isPulsar ? GetPulsarPreset() : GetBlackHolePreset();
	}

	Application::CameraState Application::GetNebulaOverview(const NebulaType type)
	{
		const NebulaPreset& preset = GetNebulaPreset(type);
		CameraState state;
		state.camera.SetZoom(preset.overviewZoom);
		TurnCamera(state.camera, preset.overviewPitch, preset.overviewYaw);
		return state;
	}

	void Application::SetNebulaType(const NebulaType type)
	{
		nebulaType = type;
		nebulaRenderer->parameters = GetNebulaPreset(type).parameters;
		const CameraState overview = GetNebulaOverview(type);
		camera = overview.camera;
		cameraOrigin = overview.origin;
		UpdateProjectionMatrix();
	}

	void Application::RenderNebulaTest(DH::CommandList& cmd, FrameUniforms& frame, const double deltaTime)
	{
		// Nothing casts shadows, and the Sun isn't drawn
		frame.occluderCount = glm::vec4(0.0f);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// The solar system's textures keep loading while it is hidden
		planetRenderer->Prepare(cmd);

		nebulaRenderer->Update(deltaTime);
		const float pixelAngle = GetPixelAngle(GetNebulaPreset(nebulaType).fieldOfView);
		nebulaRenderer->Render(cmd, *mViewportFramebuffer, NebulaView{ glm::vec3(-cameraOrigin), pixelAngle });
	}

	void Application::RenderNebulaControls()
	{
		if (!ImGui::CollapsingHeader("Nebula", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		NebulaParameters& p = nebulaRenderer->parameters;
		const NebulaParameters& defaults = GetNebulaPreset(nebulaType).parameters;
		const bool isRemnant = p.kind == NebulaKind::SupernovaRemnant;
		const bool isShell = p.kind == NebulaKind::SupernovaShell;
		const bool isEmission = p.kind == NebulaKind::HIIRegion || p.kind == NebulaKind::EmissionNebula;
		ImGui::Checkbox("Show labels (L)", &showLabels);
		ImGui::SliderFloat("Brightness", &p.brightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Bloom", &p.bloom, 0.0f, 2.0f);

		ImGui::SeparatorText("Emission");
		ImGui::Checkbox("Hubble palette", &p.hubblePalette);
		ImGui::SetItemTooltip("Show the emission lines as in many of Hubble's images: [S II] as red, hydrogen as green and [O III] as blue.\n"
							  "Otherwise they are shown in their own colours.");
		ImGui::SliderFloat("Hydrogen", &p.hydrogen, 0.0f, 3.0f);
		ImGui::SetItemTooltip("H-alpha (red) and H-beta (blue-green), from hydrogen atoms recombining");
		ImGui::SliderFloat("Oxygen", &p.oxygen, 0.0f, 3.0f);
		ImGui::SetItemTooltip("[O III] (green-blue), from doubly ionised oxygen: where the light ionising the gas is hardest");
		ImGui::SliderFloat("Nitrogen and sulphur", &p.sulphur, 0.0f, 3.0f);
		ImGui::SetItemTooltip("[N II] and [S II] (deep red)");
		if (isRemnant || isEmission)
		{
			ImGui::SliderFloat(isRemnant ? "Synchrotron" : "Scattered starlight", &p.continuum, 0.0f, 3.0f);
			ImGui::SetItemTooltip(isRemnant ? "The blue-white glow of electrons spiralling in the magnetic field, spun up by the pulsar"
											: "The stars' light scattered off the dust (blue)");
		}
		ImGui::SliderFloat("Dust", &p.dust, 0.0f, 4.0f);
		ImGui::SetItemTooltip("How much the dust dims (and reddens) what is behind it");

		ImGui::SeparatorText("Structure");
		// The nebulae range from a fraction of a parsec across to tens of parsecs: the scales of
		// their structure are set about their presets'
		ImGui::SliderFloat("Detail", &p.detail, 0.25f * defaults.detail, 4.0f * defaults.detail, "%.3g", ImGuiSliderFlags_Logarithmic);
		ImGui::SetItemTooltip("Larger for finer structure in the gas");
		ImGui::SliderFloat("Turbulence", &p.turbulence, 0.0f, 4.0f * defaults.turbulence, "%.3g pc");
		if (isRemnant)
		{
			ImGui::SliderFloat("Filaments", &p.filaments, 0.0f, 3.0f);
			ImGui::SliderFloat("Filament sharpness", &p.filamentSharpness, 2.0f, 30.0f, "%.0f");
		}
		else if (isShell)
		{
			ImGui::SliderFloat("Filaments", &p.filaments, 0.0f, 3.0f);
			ImGui::SetItemTooltip("The glow of the gas the shock has swept up");
			ImGui::SliderFloat("Sheet sharpness", &p.filamentSharpness, 20.0f, 200.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SetItemTooltip("How thin the shock's sheet is (its radius over its thickness): its folds seen edge on are the filaments");
		}
		else if (p.kind == NebulaKind::EmissionNebula)
		{
			ImGui::SliderFloat("Wisps", &p.filaments, 0.0f, 1.0f);
			ImGui::SetItemTooltip("How much of the gas is drawn out into wisps rather than clumps");
			ImGui::SliderFloat("Wisp sharpness", &p.filamentSharpness, 1.0f, 12.0f, "%.1f");
		}
		ImGui::SliderFloat("Stars", &p.starFieldBrightness, 0.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("Resolution", &p.renderScale, 0.25f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("The fraction of the viewport's resolution the gas is raymarched at");

		if (ImGui::Button("Defaults"))
			p = GetNebulaPreset(nebulaType).parameters;
	}

	void Application::DrawNebulaLabels()
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float fontSize = ImGui::GetFontSize();
		for (const NebulaStar& star : nebulaRenderer->parameters.stars)
		{
			if (star.name.empty())
				continue;
			const glm::vec4 clip = viewProjection * glm::vec4(star.position - glm::vec3(cameraOrigin), 1.0f);
			if (clip.w <= 0.0f)
				continue;
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			const glm::vec2 point = origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size;
			const ImU32 colour = IM_COL32(220, 220, 220, 210);
			const float ringRadius = LABEL_DOT_RADIUS * 2.5f;
			drawList->AddCircle(ImVec2(point.x, point.y), ringRadius, colour, 0, 1.5f);
			drawList->AddText(ImVec2(point.x + ringRadius + 4.0f, point.y - fontSize * 0.5f), colour, star.name.c_str());
		}
	}

	Application::CameraState Application::GetStarSystemOverview(const StarSystemType type)
	{
		const StarSystemPreset& preset = GetStarSystemPreset(type);
		CameraState state;
		state.camera.SetZoom(preset.overviewZoom);
		TurnCamera(state.camera, preset.overviewPitch, 0.0f);
		return state;
	}

	void Application::SetStarSystemType(const StarSystemType type)
	{
		starSystemType = type;
		const StarSystemPreset& preset = GetStarSystemPreset(type);
		starSystemRenderer->SetSystem(preset);
		starFocus = (size_t)preset.focus;
		const CameraState overview = GetStarSystemOverview(type);
		camera = overview.camera;
		cameraOrigin = starSystemRenderer->GetNodePosition((size_t)preset.focus);
		UpdateProjectionMatrix();
	}

	void Application::RenderStarTest(DH::CommandList& cmd, FrameUniforms& frame)
	{
		// Nothing casts shadows, and the Sun isn't drawn
		frame.occluderCount = glm::vec4(0.0f);
		frameUniformBuffer->Update(&frame, sizeof(frame));

		// The solar system's textures keep loading while it is hidden
		planetRenderer->Prepare(cmd);

		// (The system was advanced before the camera, so the camera follows its bodies to where they are)
		starSystemRenderer->Render(cmd, *mViewportFramebuffer, StarSystemView{ cameraOrigin, GetPixelAngle(STAR_FOV) });
	}

	void Application::RenderStarControls()
	{
		if (!ImGui::CollapsingHeader("Stars", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		StarSystemRenderer& renderer = *starSystemRenderer;
		const StarSystemPreset& system = renderer.GetSystem();
		ImGui::Checkbox("Show labels (L)", &showLabels);
		ImGui::SliderFloat("Brightness", &renderer.brightness, 1e-4f, 1e8f, "%.3g", ImGuiSliderFlags_Logarithmic);
		ImGui::SetItemTooltip("The exposure: in visible light, stars' surfaces range from millions of times fainter than the Sun's "
							  "(brown dwarfs, which give out almost all their light in the infrared) to tens of times brighter "
							  "(blue stars and white dwarfs)");
		ImGui::SliderFloat("Bloom", &renderer.bloom, 0.0f, 2.0f);
		ImGui::SliderFloat("Time scale", &renderer.daysPerSecond, 0.001f, 10000.0f, "%.3g days/s", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("Stylised stars", &renderer.stylised);
		ImGui::SetItemTooltip("Draws the stars (not brown dwarfs) as the solar system draws the Sun: coloured by their temperatures "
							  "(the Sun is white), with coronas as a total eclipse shows them. Off, they are drawn physically.");
		if (!renderer.stylised)
			ImGui::SliderFloat("Surface detail", &renderer.surfaceDetail, 0.0f, 1.5f);
		ImGui::SetItemTooltip("The contrast of the granulation, spots and cloud bands. The Sun's granules are tiny: zoom in close to its surface to see them.");
		ImGui::SliderFloat("Point stars", &renderer.pointGain, 0.0f, 200.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::SetItemTooltip("Raises the light of stars too far away to see as discs, so they show as points.\n"
							  "0 gives their true share of a pixel's light, which is lost at an exposure for stars' surfaces.");
		ImGui::SliderFloat("Background stars", &renderer.starFieldBrightness, 0.0f, 10.0f, "%.2f");
		ImGui::TextDisabled("Day %.1f", renderer.GetTime());

		// The bodies and pairs, each to follow
		ImGui::SeparatorText("Follow");
		for (size_t node = 0; node < system.nodes.size(); node++)
		{
			const StarSystemNode& n = system.nodes[node];
			ImGui::PushID((int)node);
			const bool following = starFocus == node;
			if (ImGui::RadioButton("##Follow", following) && !following)
				starFocus = node;
			ImGui::SameLine();
			if (n.body >= 0)
			{
				const StellarBody& body = system.bodies[(size_t)n.body];
				ImGui::Text("%s", body.name.c_str());
				ImGui::SetItemTooltip("%s\n%.0f K, %.3g solar radii (%.3g km), %.3g solar masses", body.description.c_str(), body.temperature,
									  body.radius, body.radius * 695700.0, body.mass);
			}
			else
			{
				ImGui::Text("%s (centre of mass)", n.name.c_str());
				ImGui::SetItemTooltip("%.3g AU apart (%.3g solar radii), every %.4g days", n.separation / SOLAR_RADII_PER_AU, n.separation, n.period);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Defaults"))
			SetStarSystemType(starSystemType);
	}

	void Application::DrawStarLabels()
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float fontSize = ImGui::GetFontSize();
		const StarSystemPreset& system = starSystemRenderer->GetSystem();
		for (size_t node = 0; node < system.nodes.size(); node++)
		{
			const int body = system.nodes[node].body;
			if (body < 0)
				continue;
			const glm::vec4 clip = viewProjection * glm::vec4(glm::vec3(starSystemRenderer->GetNodePosition(node) - cameraOrigin), 1.0f);
			if (clip.w <= 0.0f)
				continue;
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			// Beside the body's disc (at least a ring's width out)
			const float discPixels = system.bodies[(size_t)body].radius / clip.w * projectionMatrix[1][1] * 0.5f * size.y;
			const glm::vec2 point = origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size;
			const float offset = std::max(discPixels, LABEL_DOT_RADIUS * 2.5f) + 4.0f;
			drawList->AddText(ImVec2(point.x + offset, point.y - fontSize * 0.5f), IM_COL32(220, 220, 220, 210), system.bodies[(size_t)body].name.c_str());
		}
	}

	double Application::GetSunDistance() const
	{
		return glm::distance(cameraOrigin + glm::dvec3(camera.GetActualPosition()), lightSource->GetPosition());
	}

	bool Application::IsSolarSystemShown() const
	{
		// (Always, without the universe around it)
		return !(showUniverse && universeRenderer != nullptr) || GetSunDistance() < SOLAR_SYSTEM_EXTENT * Universe::AU_IN_UNITS;
	}

	void Application::RenderUniverse(DH::CommandList& cmd, const double deltaTime)
	{
		universeTime += deltaTime;
		// In parsecs from the Sun
		const glm::dvec3 sun = lightSource->GetPosition();
		UniverseView& view = universeView;
		view.origin = (cameraOrigin + glm::dvec3(camera.GetCentrePosition()) - sun) / PARSEC_IN_UNITS;
		view.offset = (glm::dvec3(camera.GetActualPosition()) - glm::dvec3(camera.GetCentrePosition())) / PARSEC_IN_UNITS;
		view.camera = view.origin + view.offset;
		view.rotation = glm::mat3(camera.GetViewMatrix());
		view.fieldOfView = glm::radians(FOV);
		view.zoom = (float)(camera.GetZoom() / PARSEC_IN_UNITS);
		view.time = universeTime;
		view.deltaTime = deltaTime;
		view.times = glm::vec4((float)animationTime, (float)animationTime * 0.05f, 0.0f, 0.0f);
		view.starExposure = exposure;
		const double sunDistance = GetSunDistance() / Universe::AU_IN_UNITS;
		const double t = std::clamp((sunDistance - SUN_POINT_START) / (SOLAR_SYSTEM_EXTENT - SUN_POINT_START), 0.0, 1.0);
		view.sunVisibility = (float)(t * t * (3.0 - 2.0 * t));
		universeRenderer->Render(cmd, *mViewportFramebuffer, view);
	}

	void Application::GoToLandmark(const CatalogueLandmark& landmark)
	{
		// (Black holes and neutron stars close up)
		const double closeUp = universeRenderer != nullptr ? universeRenderer->GetLandmarkZoom(landmark) : 0.0;
		const double zoom = closeUp > 0.0 ? closeUp : landmark.radius > 0.0f ? LANDMARK_ZOOM_RADII * landmark.radius : LANDMARK_POINT_ZOOM;
		MoveTo(lightSource->GetPosition() + landmark.position * PARSEC_IN_UNITS, (float)(zoom * PARSEC_IN_UNITS));
	}

	void Application::DrawUniverseLabels()
	{
		if (catalogue == nullptr)
			return;
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		if (size.x <= 0.0f || size.y <= 0.0f)
			return;
		const float pixelAngle = 2.0f / (projectionMatrix[1][1] * size.y);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float fontSize = ImGui::GetFontSize();

		// The prominent landmarks on screen, most prominent first
		struct Candidate
		{
			float prominence;
			size_t index;
			glm::vec2 point;
		};
		std::vector<Candidate> candidates;
		// Inside the solar system, the sky behind its bodies is hidden
		const bool inSolarSystem = IsSolarSystemShown();
		const std::vector<glm::vec3> discs = inSolarSystem ? GetBodyDiscs() : std::vector<glm::vec3>();
		for (size_t i = 0; i < catalogue->landmarks.size(); i++)
		{
			const CatalogueLandmark& landmark = catalogue->landmarks[i];
			const std::optional<glm::vec2> point = ProjectLandmark(landmark.position);
			if (!point || point->x < origin.x || point->y < origin.y || point->x > origin.x + size.x || point->y > origin.y + size.y)
				continue;
			const float prominence = universeRenderer->GetProminence(landmark, universeView, pixelAngle);
			if (prominence < 0.0f)
				continue;
			bool hidden = false;
			for (const glm::vec3& disc : discs)
				hidden = hidden || glm::distance(glm::vec2(disc), *point) < disc.z;
			if (!hidden)
				candidates.push_back({ prominence, i, *point });
		}
		std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.prominence > b.prominence; });

		// (Around the bodies' labels, drawn first)
		std::vector<ImVec4> placed;
		for (const auto& [rect, index] : labelRects)
			placed.emplace_back(rect.x, rect.y, rect.z, rect.w);
		auto Place = [&](const glm::vec2 point, const char* name, const ImU32 colour) -> std::optional<ImVec4>
		{
			const ImVec2 textSize = ImGui::CalcTextSize(name);
			const float offset = LABEL_DOT_RADIUS + 4.0f;
			const ImVec4 rect(point.x - LABEL_DOT_RADIUS - LABEL_PADDING, point.y - fontSize * 0.5f - LABEL_PADDING,
							  point.x + offset + textSize.x + LABEL_PADDING, point.y + fontSize * 0.5f + LABEL_PADDING);
			for (const ImVec4& other : placed)
			{
				if (rect.x < other.z && rect.z > other.x && rect.y < other.w && rect.w > other.y)
					return std::nullopt;
			}
			placed.push_back(rect);
			drawList->AddCircle(ImVec2(point.x, point.y), LABEL_DOT_RADIUS + 1.5f, colour, 0, 1.0f);
			drawList->AddText(ImVec2(point.x + offset, point.y - fontSize * 0.5f), colour, name);
			return rect;
		};
		// The Sun, while the solar system (with its own labels) isn't drawn
		if (!inSolarSystem)
		{
			const std::optional<glm::vec2> point = ProjectLandmark(glm::dvec3(0.0));
			if (point && point->x >= origin.x && point->y >= origin.y && point->x <= origin.x + size.x && point->y <= origin.y + size.y)
				Place(*point, "Sun", IM_COL32(255, 236, 190, 230));
		}

		auto Colour = [](const CatalogueKind kind) -> ImU32
		{
			switch (kind)
			{
			case CatalogueKind::Star: return IM_COL32(225, 225, 235, 210);
			case CatalogueKind::Galaxy: return IM_COL32(235, 215, 170, 210);
			case CatalogueKind::PlanetaryNebula: return IM_COL32(150, 235, 215, 210);
			case CatalogueKind::HIIRegion: return IM_COL32(245, 160, 175, 210);
			case CatalogueKind::SupernovaRemnant: return IM_COL32(245, 190, 150, 210);
			case CatalogueKind::BlackHole: return IM_COL32(255, 200, 120, 230);
			default: return IM_COL32(150, 200, 255, 210);
			}
		};
		size_t count = 0;
		// (Fewer while the solar system has its own labels, unless the camera is past its planets)
		const size_t maxLabels = inSolarSystem && camera.GetZoom() < SUN_POINT_START * Universe::AU_IN_UNITS ? MAX_SKY_LABELS : MAX_LANDMARK_LABELS;
		for (const Candidate& candidate : candidates)
		{
			if (count >= maxLabels)
				break;
			const CatalogueLandmark& landmark = catalogue->landmarks[candidate.index];
			if (const std::optional<ImVec4> rect = Place(candidate.point, landmark.name.c_str(), Colour(landmark.kind)))
			{
				landmarkLabelRects.push_back({ glm::vec4(rect->x, rect->y, rect->z, rect->w), candidate.index });
				count++;
			}
		}
	}

	bool Application::UpdateLandmarkSelection()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		auto OverImage = [&](const ImVec2& p)
		{
			return p.x >= origin.x && p.y >= origin.y && p.x < origin.x + size.x && p.y < origin.y + size.y;
		};
		bool clicked = false;
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy) && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left) && OverImage(io.MouseClickedPos[ImGuiMouseButton_Left]) && OverImage(io.MousePos))
		{
			const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
			for (const auto& [rect, index] : landmarkLabelRects)
			{
				if (mouse.x >= rect.x && mouse.x <= rect.z && mouse.y >= rect.y && mouse.y <= rect.w)
				{
					selectedLandmark = index;
					clicked = true;
					break;
				}
			}
			// Otherwise the sky near the cursor, unless a body of the solar system (or its label) is
			// there
			if (!clicked && showUniverse && universeRenderer != nullptr && catalogue != nullptr)
			{
				bool overBody = false;
				if (IsSolarSystemShown())
				{
					for (const auto& [rect, index] : labelRects)
						overBody = overBody || (mouse.x >= rect.x && mouse.x <= rect.z && mouse.y >= rect.y && mouse.y <= rect.w);
					overBody = overBody || PickBody((mouse - origin) / size).has_value();
				}
				if (!overBody)
				{
					if (const std::optional<size_t> picked = PickLandmark(mouse))
					{
						selectedLandmark = *picked;
						clicked = true;
					}
				}
			}
			if (clicked)
				ImGui::OpenPopup(LANDMARK_MENU);
		}

		// Rings a landmark (a dark outline under a light line, as a selected body), returning the
		// ring's centre and radius
		auto Ring = [&](const CatalogueLandmark& landmark, const ImU32 colour) -> std::optional<glm::vec3>
		{
			const std::optional<glm::vec2> point = ProjectLandmark(landmark.position);
			if (!point)
				return std::nullopt;
			const float pixelAngle = 2.0f / (projectionMatrix[1][1] * std::max(size.y, 1.0f));
			const double distance = std::max(glm::distance(landmark.position, universeView.camera), 1e-12);
			const float discRadius = (float)(landmark.radius / distance / pixelAngle);
			const float ringRadius = std::min(std::max(discRadius, LABEL_DOT_RADIUS), 0.45f * size.y) + SELECTION_RING_GAP;
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddCircle(ImVec2(point->x, point->y), ringRadius, IM_COL32(0, 0, 0, 140), 0, 3.5f);
			drawList->AddCircle(ImVec2(point->x, point->y), ringRadius, colour, 0, 1.5f);
			return glm::vec3(*point, ringRadius);
		};

		// Hovering over the sky rings the landmark a click there would pick, and names it, so what can
		// be clicked on shows (many are too faint or small to see from the solar system). Not while a
		// button is down (turning the camera), a menu is open, or over a label or a body.
		const bool menuOpen = ImGui::IsPopupOpen(LANDMARK_MENU) || ImGui::IsPopupOpen(BODY_MENU);
		if (!clicked && !menuOpen && showUniverse && universeRenderer != nullptr && catalogue != nullptr && OverImage(io.MousePos) &&
			ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy) && !ImGui::IsAnyMouseDown())
		{
			const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
			auto Over = [&](const std::vector<std::pair<glm::vec4, size_t>>& rects)
			{
				for (const auto& [rect, index] : rects)
				{
					if (mouse.x >= rect.x && mouse.x <= rect.z && mouse.y >= rect.y && mouse.y <= rect.w)
						return true;
				}
				return false;
			};
			const bool overBody = IsSolarSystemShown() && (Over(labelRects) || PickBody((mouse - origin) / size).has_value());
			if (!overBody && !Over(landmarkLabelRects))
			{
				if (const std::optional<size_t> hovered = PickLandmark(mouse))
				{
					const CatalogueLandmark& landmark = catalogue->landmarks[*hovered];
					if (const std::optional<glm::vec3> ring = Ring(landmark, IM_COL32(235, 235, 235, 150)))
					{
						const char* kind = GetCatalogueKindName(landmark.kind);
						const float fontSize = ImGui::GetFontSize();
						const ImVec2 at(ring->x + ring->z + 5.0f, ring->y - fontSize * 0.5f);
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						const std::string text = landmark.name + "  (" + kind + ")";
						drawList->AddText(ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, 200), text.c_str());
						drawList->AddText(at, IM_COL32(235, 235, 235, 230), text.c_str());
					}
				}
			}
		}

		if (catalogue == nullptr || !selectedLandmark || !ImGui::IsPopupOpen(LANDMARK_MENU))
		{
			selectedLandmark.reset();
			return clicked;
		}
		const CatalogueLandmark& landmark = catalogue->landmarks[*selectedLandmark];

		// Ring the selected landmark while its menu is open
		Ring(landmark, IM_COL32(235, 235, 235, 230));

		if (ImGui::BeginPopup(LANDMARK_MENU))
		{
			ImGui::TextUnformatted(landmark.name.c_str());
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
			ImGui::TextDisabled("%s", landmark.description.c_str());
			ImGui::PopTextWrapPos();
			ImGui::Separator();
			if (ImGui::MenuItem("Go to object"))
				GoToLandmark(landmark);
			ImGui::EndPopup();
		}
		return clicked;
	}

	std::optional<glm::vec2> Application::ProjectLandmark(const glm::dvec3& position) const
	{
		const glm::vec3 relative = universeView.rotation * glm::vec3(position - universeView.camera);
		if (relative.z >= 0.0f)
			return std::nullopt; // Behind the camera
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		const glm::vec2 ndc(projectionMatrix[0][0] * relative.x / -relative.z, projectionMatrix[1][1] * relative.y / -relative.z);
		return origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size;
	}

	std::vector<glm::vec3> Application::GetBodyDiscs() const
	{
		const glm::vec2 origin = GetViewportImageOrigin();
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		std::vector<glm::vec3> discs;
		for (const NamedBody& namedBody : namedBodies)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(namedBody.body->GetTranslation(), 1.0f);
			if (clip.w <= 0.0f)
				continue;
			const float radius = GetOuterRadius(*namedBody.body) / clip.w * projectionMatrix[1][1] * size.y * 0.5f;
			if (radius < 1.0f)
				continue;
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			discs.emplace_back(origin + glm::vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f) * size, radius);
		}
		return discs;
	}

	std::optional<size_t> Application::PickLandmark(const glm::vec2 point) const
	{
		const glm::vec2 size((float)mViewportFramebuffer->GetWidth(), (float)mViewportFramebuffer->GetHeight());
		const float pixelAngle = 2.0f / (projectionMatrix[1][1] * std::max(size.y, 1.0f));
		const std::vector<glm::vec3> discs = IsSolarSystemShown() ? GetBodyDiscs() : std::vector<glm::vec3>();
		// Objects within a few pixels of the cursor first (the more prominent, then the nearer it),
		// and only then one whose disc the cursor is inside (the smallest), so a star or a small
		// nebula in front of a large galaxy is picked by clicking on it
		std::optional<size_t> picked, pickedDisc;
		float best = -std::numeric_limits<float>::max();
		float smallestDisc = std::numeric_limits<float>::max();
		for (size_t i = 0; i < catalogue->landmarks.size(); i++)
		{
			const CatalogueLandmark& landmark = catalogue->landmarks[i];
			// Not one the camera is inside (the Milky Way, from the solar system): its "disc" would be
			// the whole sky, and take every click
			const double distance = std::max(glm::distance(landmark.position, universeView.camera), 1e-12);
			if (distance < landmark.radius)
				continue;
			const std::optional<glm::vec2> where = ProjectLandmark(landmark.position);
			if (!where)
				continue;
			const float discRadius = (float)(landmark.radius / distance / pixelAngle);
			const float miss = glm::distance(*where, point);
			const bool close = miss <= LANDMARK_PICK_PIXELS;
			if (!close && miss > discRadius)
				continue;
			if (!close && pickedDisc && discRadius >= smallestDisc)
				continue; // (Cheaper than the checks below)
			bool hidden = false;
			for (const glm::vec3& disc : discs)
				hidden = hidden || glm::distance(glm::vec2(disc), *where) < disc.z;
			if (hidden)
				continue;
			const float prominence = universeRenderer->GetProminence(landmark, universeView, pixelAngle, true);
			if (prominence < 0.0f)
				continue;
			if (!close)
			{
				smallestDisc = discRadius;
				pickedDisc = i;
				continue;
			}
			// (Prominence is capped, so a very prominent object, such as Sagittarius A*, doesn't take
			// clicks on the stars around it)
			const float score = std::min(prominence, LANDMARK_PICK_MAX_PROMINENCE) - 2.0f * miss / LANDMARK_PICK_PIXELS;
			if (score > best)
			{
				best = score;
				picked = i;
			}
		}
		return picked ? picked : pickedDisc;
	}

	void Application::RenderUniverseControls()
	{
		if (!ImGui::CollapsingHeader("Universe", ImGuiTreeNodeFlags_DefaultOpen))
			return;
		ImGui::Checkbox("Show the universe", &showUniverse);
		ImGui::SetItemTooltip("The stars, the Milky Way, other galaxies, nebulae, pulsars and black holes of the catalogues, seen as you zoom out of the solar system");
		if (universeRenderer == nullptr)
		{
			ImGui::TextDisabled(pendingCatalogue.valid() ? "Loading the catalogues..." : "No catalogues loaded");
			return;
		}
		if (!showUniverse)
			return;

		UniverseRenderer& universe = *universeRenderer;
		ImGui::Checkbox("Stars", &universe.showStars);
		ImGui::SameLine();
		ImGui::Checkbox("Milky Way", &universe.showMilkyWay);
		ImGui::SameLine();
		ImGui::Checkbox("Galaxies", &universe.showGalaxies);
		ImGui::Checkbox("Nebulae", &universe.showNebulae);
		ImGui::SameLine();
		ImGui::Checkbox("Neutron stars", &universe.showNeutronStars);
		ImGui::SameLine();
		ImGui::Checkbox("Black holes", &universe.showBlackHoles);
		ImGui::SliderFloat("Sky exposure", &universe.exposure, 0.5f, 200.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Star brightness", &universe.starBrightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("Brighten as you zoom out", &universe.zoomGain);
		ImGui::SetItemTooltip("Raises the light of distant stars and galaxies as the camera zooms out past them, so they still show (as a camera's exposure would)");
		ImGui::SliderFloat("Milky Way brightness", &universe.milkyWayBrightness, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Galaxy brightness", &universe.galaxyBrightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Nebula brightness", &universe.nebulaBrightness, 0.05f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Markers", &universe.markerBrightness, 0.0f, 5.0f, "%.2f");
		ImGui::SetItemTooltip("The brightness of the markers for pulsars, neutron stars and black holes (which give out next to no light that could be seen), and for distant nebulae");
		ImGui::SliderFloat("Bloom##Universe", &universe.bloom, 0.0f, 2.0f);

		// Finding a landmark by name, and some to start with
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Find", "Find a star, nebula, galaxy or pulsar", landmarkFilter.data(), landmarkFilter.size());
		const std::string filter = landmarkFilter.data();
		if (!filter.empty())
		{
			auto Lower = [](std::string text)
			{
				std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char c) { return (char)std::tolower(c); });
				return text;
			};
			const std::string needle = Lower(filter);
			if (ImGui::BeginListBox("##Results", ImVec2(-FLT_MIN, 8.0f * ImGui::GetTextLineHeightWithSpacing())))
			{
				size_t shown = 0;
				for (size_t i = 0; i < catalogue->landmarks.size() && shown < MAX_SEARCH_RESULTS; i++)
				{
					const CatalogueLandmark& landmark = catalogue->landmarks[i];
					if (Lower(landmark.name).find(needle) == std::string::npos && Lower(landmark.description).find(needle) == std::string::npos)
						continue;
					ImGui::PushID((int)i);
					if (ImGui::Selectable(landmark.name.c_str()))
						GoToLandmark(landmark);
					ImGui::SetItemTooltip("%s\nClick to go to it", landmark.description.c_str());
					ImGui::PopID();
					shown++;
				}
				if (shown == 0)
					ImGui::TextDisabled("Nothing found");
				ImGui::EndListBox();
			}
		}
		else
		{
			ImGui::TextDisabled("Go to:");
			for (const char* name : { "Sagittarius A*", "Andromeda Galaxy", "Large Magellanic Cloud", "Orion Nebula", "Crab Nebula", "PSR B0531+21 (Crab)", "Betelgeuse" })
			{
				for (const CatalogueLandmark& landmark : catalogue->landmarks)
				{
					if (landmark.name != name)
						continue;
					if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(name).x + 2.0f * ImGui::GetStyle().FramePadding.x)
						ImGui::NewLine();
					if (ImGui::SmallButton(name))
						GoToLandmark(landmark);
					ImGui::SetItemTooltip("%s", landmark.description.c_str());
					ImGui::SameLine();
					break;
				}
			}
			ImGui::NewLine();
			if (ImGui::SmallButton("The Milky Way"))
				MoveTo(lightSource->GetPosition() + GetGalacticCentre() * PARSEC_IN_UNITS, (float)(60000.0 * PARSEC_IN_UNITS));
			ImGui::SetItemTooltip("Our galaxy, from outside");
			ImGui::SameLine();
			if (ImGui::SmallButton("The Local Group"))
				MoveTo(lightSource->GetPosition(), (float)(3.0e6 * PARSEC_IN_UNITS));
			ImGui::SameLine();
			if (ImGui::SmallButton("The nearby universe"))
				MoveTo(lightSource->GetPosition(), (float)(2.5e8 * PARSEC_IN_UNITS));
		}

		if (ImGui::TreeNode("Catalogues"))
		{
			for (const std::string& line : catalogue->report)
				ImGui::BulletText("%s", line.c_str());
			ImGui::TreePop();
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
			constexpr const char* SCENE_NAMES[] = { "Solar System", "Galaxy Test", "Black Hole Test", "Pulsar Test", "Nebula Test", "Star Test" };
			static_assert(std::size(SCENE_NAMES) == (size_t)SceneMode::Count);
			int mode = (int)sceneMode;
			if (ImGui::Combo("Scene##Mode", &mode, SCENE_NAMES, (int)SceneMode::Count))
				SetSceneMode((SceneMode)mode);
			ImGui::SetItemTooltip("The tests show only a galaxy, a black hole, a pulsar, a nebula or stars, in place of the solar system");
			if (sceneMode == SceneMode::Galaxy)
			{
				const GalaxyPreset& current = GetGalaxyPreset(galaxyType);
				if (ImGui::BeginCombo("Galaxy##Preset", current.name.c_str()))
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
			if (sceneMode == SceneMode::Stars)
			{
				const StarSystemPreset& current = GetStarSystemPreset(starSystemType);
				if (ImGui::BeginCombo("Stars##Preset", current.name.c_str()))
				{
					for (size_t i = 0; i < (size_t)StarSystemType::Count; i++)
					{
						const StarSystemType type = (StarSystemType)i;
						const bool selected = type == starSystemType;
						if (ImGui::Selectable(GetStarSystemPreset(type).name.c_str(), selected) && !selected)
							SetStarSystemType(type);
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			if (sceneMode == SceneMode::Nebula)
			{
				const NebulaPreset& current = GetNebulaPreset(nebulaType);
				if (ImGui::BeginCombo("Nebula##Preset", current.name.c_str()))
				{
					for (size_t i = 0; i < (size_t)NebulaType::Count; i++)
					{
						const NebulaType type = (NebulaType)i;
						const bool selected = type == nebulaType;
						if (ImGui::Selectable(GetNebulaPreset(type).name.c_str(), selected) && !selected)
							SetNebulaType(type);
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			if (sceneMode == SceneMode::SolarSystem)
			{
				ImGui::DragFloat("Exposure (-/=)", &exposure, 0.1f, 0.0f, 100.0f);
				int surfaceView = (int)starRenderer->surfaceView;
				const char* surfaceViews[] = { "Visible light", "Extreme ultraviolet", "Stylised" };
				if (ImGui::Combo("Star surfaces", &surfaceView, surfaceViews, 3))
					starRenderer->surfaceView = (StarSurfaceView)surfaceView;
				ImGui::SetItemTooltip("Visible light: the photosphere as through a solar filter, darker and redder towards the limb, "
									  "with sunspots, faculae and granulation (zoom in close to see the granules).\n"
									  "Extreme ultraviolet: the hot, glowing chromosphere and corona above it, as SDO sees them.\n"
									  "Stylised: a cinematic take on a star, coloured by its temperature (the Sun is white), with its corona as "
									  "a total eclipse shows it.\n"
									  "Every star close up (the catalogue's as you approach them) is drawn this way.");
				if (starRenderer->surfaceView == StarSurfaceView::VisibleLight)
					ImGui::SliderFloat("Sunspots", &starRenderer->sunspots, 0.0f, 1.5f);
				ImGui::SliderFloat("H-alpha emission", &starRenderer->hydrogenEmission, 0.0f, 4.0f);
				if (starRenderer->surfaceView != StarSurfaceView::Stylised)
				{
					ImGui::SliderFloat("Solar flares", &starRenderer->solarFlares, 0.0f, 2.0f);
					ImGui::SetItemTooltip("Brightness of the flares on the Sun's surface and in the loops over its limb (0 for none)");
				}
				ImGui::SliderFloat("Star flares", &starRenderer->flareIntensity, 0.0f, 2.0f);
				ImGui::SetItemTooltip("The glare and lens flare of the Sun, and of other stars as the camera nears them, while they are small on screen (0 for none)");
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

		if (sceneMode == SceneMode::Galaxy)
			RenderGalaxyControls();
		if (sceneMode == SceneMode::BlackHole || sceneMode == SceneMode::Pulsar)
			RenderCompactObjectControls();
		if (sceneMode == SceneMode::Nebula)
			RenderNebulaControls();
		if (sceneMode == SceneMode::Stars)
			RenderStarControls();

		if (sceneMode == SceneMode::SolarSystem)
			RenderUniverseControls();

		if (sceneMode == SceneMode::SolarSystem && ImGui::CollapsingHeader("Bodies", ImGuiTreeNodeFlags_DefaultOpen))
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
			if (sceneMode == SceneMode::Galaxy)
			{
				const glm::dvec3 kpc = centre / 1000.0;
				ImGui::Text("Centre (kpc): X=%.3f, Y=%.3f, Z=%.3f", kpc.x, kpc.y, kpc.z);
				ImGui::Text("Zoom: %.3g kpc", camera.GetZoom() / 1000.0f);
			}
			else if (sceneMode == SceneMode::Stars)
			{
				ImGui::Text("Centre (solar radii): X=%.4g, Y=%.4g, Z=%.4g", centre.x, centre.y, centre.z);
				ImGui::Text("Zoom: %.3g solar radii (%.3g AU)", camera.GetZoom(), camera.GetZoom() / SOLAR_RADII_PER_AU);
				if (starFocus.has_value())
					ImGui::Text("Following %s", starSystemRenderer->GetSystem().nodes[*starFocus].name.c_str());
			}
			else if (sceneMode == SceneMode::Nebula)
			{
				ImGui::Text("Centre (pc): X=%.2f, Y=%.2f, Z=%.2f", centre.x, centre.y, centre.z);
				ImGui::Text("Zoom: %.3g pc", camera.GetZoom());
			}
			else if (sceneMode == SceneMode::BlackHole || sceneMode == SceneMode::Pulsar)
			{
				ImGui::Text("Centre (Schwarzschild radii): X=%.2f, Y=%.2f, Z=%.2f", centre.x, centre.y, centre.z);
				ImGui::Text("Zoom: %.3g Schwarzschild radii", camera.GetZoom());
			}
			else
			{
				// Lengths in the units that suit them: Earth radii and AU in the solar system, light
				// years and parsecs past it
				auto FormatLength = [](const double units)
				{
					const double au = units / Universe::AU_IN_UNITS, pc = units / PARSEC_IN_UNITS;
					char text[96];
					if (au < 1000.0)
						std::snprintf(text, sizeof(text), "%.3g Earth radii (%.3g AU)", units, au);
					else if (pc < 1000.0)
						std::snprintf(text, sizeof(text), "%.3g pc (%.3g light years)", pc, pc * 3.26156);
					else if (pc < 1.0e6)
						std::snprintf(text, sizeof(text), "%.3g kpc (%.3g thousand light years)", pc / 1e3, pc * 3.26156e-3);
					else
						std::snprintf(text, sizeof(text), "%.3g Mpc (%.3g million light years)", pc / 1e6, pc * 3.26156e-6);
					return std::string(text);
				};
				const glm::dvec3 au = centre / Universe::AU_IN_UNITS;
				if (glm::length(au) < 1.0e5)
					ImGui::Text("Centre (AU): X=%.4f, Y=%.4f, Z=%.4f", au.x, au.y, au.z);
				else
					ImGui::Text("Centre: %s from the Sun", FormatLength(glm::distance(centre, lightSource->GetPosition())).c_str());
				ImGui::Text("Zoom: %s", FormatLength(camera.GetZoom()).c_str());
			}
			ImGui::Text("Pitch: %.1f, Yaw: %.1f", camera.GetPitch(), camera.GetYaw());
			for (const NamedBody& namedBody : namedBodies)
			{
				if (namedBody.body == followBody)
					ImGui::Text("Following %s", namedBody.name.c_str());
			}
			if (ImGui::Button("Reset camera"))
			{
				if (sceneMode != SceneMode::SolarSystem)
				{
					const CameraState overview = GetTestOverview();
					camera = overview.camera;
					cameraOrigin = overview.origin;
					if (sceneMode == SceneMode::Stars)
						starFocus = (size_t)GetStarSystemPreset(starSystemType).focus;
				}
				else
				{
					MoveTo(lightSource, (float)OVERVIEW_ZOOM);
				}
			}
			ImGui::TextWrapped("Trackpad: two finger scroll to orbit, pinch or Cmd + scroll to zoom, Shift + scroll to pan.");
			ImGui::TextWrapped("Mouse: drag to orbit, right drag or Option + drag to pan, wheel or Shift + drag to zoom.");
			ImGui::TextWrapped("W/A/S/D: move (slowing near a surface, to see it close up). Panning or moving stops following a body.");
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
		// Not while typing (WantCaptureKeyboard would also block them whenever any window, the
		// viewport included, is focused, as ImGui's keyboard navigation is enabled)
		if (ImGui::GetIO().WantTextInput)
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
