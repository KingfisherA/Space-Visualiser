#include "CompactObject.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/Buffer.h"
#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Graphics/Framebuffer.h"
#include "Dahlia/Graphics/GraphicsPipeline.h"
#include "SV/Graphics/ShaderSources.hpp"
#include "Star.hpp"

namespace SV
{
	namespace
	{
		// Mirrors CompactObjectUniforms in Shaders/CompactObject.frag (std140)
		struct CompactObjectUniforms
		{
			glm::vec4 centre;   // xyz = the object's centre (render coordinates), w = time
			glm::vec4 disc;     // x = inner radius, y = outer radius (0 for no disc), z = opacity, w = brightness
			glm::vec4 discLook; // x = Doppler beaming, y = gravitational redshift, z = peak temperature, w = turbulence
			glm::vec4 jets;     // x = brightness (0 for none), y = speed (c), z = tan(half opening angle), w = length
			glm::vec4 sky;      // x = star brightness, y = sky brightness, z = pixel angle, w = escape radius
			glm::vec4 star;     // x = radius (0 for a black hole), y = brightness, z = temperature, w = cos(hot spot radius)
			glm::vec4 magnetic; // xyz = the magnetic axis, w = hot spot brightness
			glm::vec4 beams;    // x = brightness (0 for none), y = half width (radians), z = length, w = magnetosphere
			std::array<glm::vec4, BLACKBODY_TABLE_SIZE> blackbody;
		};

		// Rays that have left this far (and are heading away) see the sky. It takes in the disc and
		// the jets or beams, and is far enough out that what bending remains is slight.
		constexpr float MIN_ESCAPE_RADIUS = 40.0f;
		constexpr float ESCAPE_MARGIN = 1.5f;

		// The time is kept within this, a multiple of the disc's flow period (FLOW_PERIOD in
		// Shaders/CompactObject.frag), so it stays precise in a float
		constexpr double TIME_WRAP = 30.0 * 200.0;
		constexpr double TAU = 2.0 * std::numbers::pi;
	}

	CompactObjectParameters GetBlackHolePreset()
	{
		return CompactObjectParameters{};
	}

	CompactObjectParameters GetPulsarPreset()
	{
		CompactObjectParameters p;
		p.type = CompactObjectType::NeutronStar;
		// The disc is held off by the star's magnetic field, and is cooler and fainter so the star
		// and its beams stand out
		p.discInnerRadius = 9.0f;
		p.discOuterRadius = 28.0f;
		p.discTemperature = 6000.0f;
		p.discBrightness = 1.2f;
		p.discOpacity = 0.7f;
		p.timeScale = 20.0f;
		p.showJets = false;
		p.starBrightness = 0.7f;
		return p;
	}

	CompactObjectRenderer::CompactObjectRenderer(const SceneContext& contextIn, const CompactObjectParameters& parametersIn) :
		parameters(parametersIn),
		context(contextIn),
		compositor(contextIn)
	{
		const CompactObjectUniforms uniforms{};
		uniformBuffer = DH::Buffer::Create({ sizeof(CompactObjectUniforms), DH::BufferUsage::Uniform, DH::BufferMemory::Dynamic }, &uniforms);

		// Set 1 of the trace: the object's parameters
		objectLayout = DH::BindGroupLayout::Create({ { 0, DH::ShaderResourceType::UniformBuffer, DH::ShaderStage::AllGraphics } });
		objectGroup = DH::BindGroup::Create(objectLayout, { DH::BindGroupEntry::UniformBuffer(0, uniformBuffer) });

		// The trace: written over the whole scene target
		DH::GraphicsPipelineDesc traceDesc;
		traceDesc.vertexShader = Shaders::Create("GalaxyGlow.vert"); // A full screen triangle with each pixel's NDC
		traceDesc.fragmentShader = Shaders::Create("CompactObject.frag");
		traceDesc.depth = { false, false, DH::CompareOp::Always };
		traceDesc.blend = DH::BlendPreset::None;
		traceDesc.targetLayout = BloomCompositor::GetSceneLayout();
		traceDesc.bindGroupLayouts = { context.frameLayout, objectLayout };
		tracePipeline = DH::GraphicsPipeline::Create(traceDesc);
	}

	CompactObjectRenderer::~CompactObjectRenderer() = default;

	void CompactObjectRenderer::Update(const double deltaTime)
	{
		time = std::fmod(time + deltaTime * (double)parameters.timeScale, TIME_WRAP);
		spinAngle = std::fmod(spinAngle + TAU * deltaTime / std::max((double)parameters.spinPeriod, 1e-3), TAU);
	}

	void CompactObjectRenderer::Render(DH::CommandList& cmd, DH::Framebuffer& target, const CompactObjectView& view)
	{
		DH::Framebuffer& scene = compositor.GetSceneTarget(target.GetWidth(), target.GetHeight(), parameters.renderScale);

		const CompactObjectParameters& p = parameters;
		const bool isStar = p.type == CompactObjectType::NeutronStar;
		const float pixelAngle = view.pixelAngle * (float)target.GetHeight() / (float)scene.GetHeight();
		const float jetReach = p.showJets ? 2.0f * p.jetLength : 0.0f;
		const float beamReach = isStar ? 2.0f * p.beamLength : 0.0f;
		// The magnetic axis, tilted from the spin axis (y) and turning about it with the star
		const float tilt = glm::radians(p.magneticTilt);
		const glm::vec3 magneticAxis(std::sin(tilt) * (float)std::cos(spinAngle), std::cos(tilt), std::sin(tilt) * (float)std::sin(spinAngle));

		const CompactObjectUniforms uniforms{
			glm::vec4(view.centre, (float)time),
			glm::vec4(p.discInnerRadius, p.showDisc ? std::max(p.discOuterRadius, p.discInnerRadius) : 0.0f, p.discOpacity, p.discBrightness),
			glm::vec4(p.dopplerBeaming, p.gravitationalRedshift, p.discTemperature, p.discTurbulence),
			glm::vec4(p.showJets ? p.jetBrightness : 0.0f, p.jetSpeed, std::tan(glm::radians(p.jetOpeningAngle)), p.jetLength),
			glm::vec4(p.starFieldBrightness, p.skyBrightness, pixelAngle,
					  ESCAPE_MARGIN * std::max({ MIN_ESCAPE_RADIUS, p.discOuterRadius, jetReach, beamReach })),
			glm::vec4(isStar ? std::max(p.starRadius, 1.05f) : 0.0f, p.starBrightness, p.starTemperature, std::cos(glm::radians(p.hotSpotSize))),
			glm::vec4(magneticAxis, p.hotSpotBrightness),
			glm::vec4(isStar ? p.beamBrightness : 0.0f, glm::radians(std::max(p.beamWidth, 0.1f)), p.beamLength, isStar ? p.magnetosphere : 0.0f),
			BlackbodyTable()
		};
		uniformBuffer->Update(&uniforms, sizeof(uniforms));

		DH::RenderPassDesc pass;
		pass.target = &scene;
		pass.clearColour = glm::vec4(0.0f);
		pass.depthLoadOp = DH::LoadOp::DontCare;
		cmd.BeginRenderPass(pass);
		cmd.BindPipeline(*tracePipeline);
		cmd.BindGroup(0, *context.frameGroup);
		cmd.BindGroup(1, *objectGroup);
		cmd.Draw(3);
		cmd.EndRenderPass();

		compositor.Composite(cmd, target, p.brightness, p.bloom, compositeMode, opacity);
	}
}
