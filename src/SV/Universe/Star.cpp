#include "Star.hpp"

#include <glm/gtx/transform.hpp>

#include "Dahlia/Graphics/CommandList.h"
#include "Dahlia/Maths/Geometry.h"
#include "SV/Graphics/SceneContext.hpp"
#include "SV/Graphics/ShaderSources.hpp"

namespace SV
{
	namespace
	{
		// Mirrors the push constants in Shaders/Star.vert and Star.frag
		struct StarPushConstants
		{
			glm::mat4 transform;
			glm::vec4 properties; // radius, temperature, noise octaves
			glm::vec4 colour;
		};

		// Mirrors the push constants in Shaders/Corona.vert and Corona.frag
		struct CoronaPushConstants
		{
			glm::mat4 transform;
			glm::vec4 colour;
		};

		constexpr unsigned int SPHERE_SUBDIVISIONS = 20;
		constexpr float STAR_NOISE_OCTAVES = 3.0f;
		// Stars are drawn slightly inside their transformation so the corona surrounds them
		constexpr float STAR_SCALE = 0.75f;
		// Coronas are sized relative to the star's scale so they aren't hidden inside larger stars
		constexpr float CORONA_SCALE = 1.8f;
	}

	StarRenderer::StarRenderer(const SceneContext& context)
	{
		sphereMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuadsphere(1.0f, SPHERE_SUBDIVISIONS));
		quadMesh = DH::GPUMesh::Create(DH::Geometry::CreateQuad());

		DH::GraphicsPipelineDesc starDesc;
		starDesc.vertexShader = Shaders::Create("Star.vert");
		starDesc.fragmentShader = Shaders::Create("Star.frag");
		// Positions and normals
		starDesc.vertexBindings = { sphereMesh->GetVertexBindings()[0], sphereMesh->GetVertexBindings()[1] };
		starDesc.vertexAttributes = { sphereMesh->GetVertexAttributes()[0], sphereMesh->GetVertexAttributes()[1] };
		starDesc.raster.cullMode = DH::CullMode::Back;
		starDesc.depth = { true, true, DH::CompareOp::Less };
		starDesc.blend = DH::BlendPreset::Alpha;
		starDesc.targetLayout = context.targetLayout;
		starDesc.bindGroupLayouts = { context.frameLayout };
		starDesc.pushConstantSize = sizeof(StarPushConstants);
		starPipeline = ModalPipeline(starDesc);

		DH::GraphicsPipelineDesc coronaDesc;
		coronaDesc.vertexShader = Shaders::Create("Corona.vert");
		coronaDesc.fragmentShader = Shaders::Create("Corona.frag");
		coronaDesc.vertexBindings = quadMesh->GetVertexBindings();
		coronaDesc.vertexAttributes = quadMesh->GetVertexAttributes();
		coronaDesc.raster.cullMode = DH::CullMode::None;
		// Tested against, but not written to, the depth buffer
		coronaDesc.depth = { true, false, DH::CompareOp::Less };
		coronaDesc.blend = DH::BlendPreset::Additive;
		coronaDesc.targetLayout = context.targetLayout;
		coronaDesc.bindGroupLayouts = { context.frameLayout };
		coronaDesc.pushConstantSize = sizeof(CoronaPushConstants);
		coronaPipeline = ModalPipeline(coronaDesc);
	}

	void StarRenderer::Render(DH::CommandList& cmd, const DH::PolygonMode polygonMode)
	{
		if (stars.empty()) return;

		// Coronas first, so the stars are drawn over them
		cmd.BindPipeline(coronaPipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			const Star& star = *stars[index];
			const CoronaPushConstants pushConstants{
				glm::translate(star.GetTranslation()) * glm::scale(CORONA_SCALE * star.GetScale()),
				glm::vec4(star.starColour, 1.0f)
			};
			cmd.PushConstants(pushConstants);
			quadMesh->Draw(cmd);
		}

		cmd.BindPipeline(starPipeline.Get(polygonMode));
		for (const auto& [distance, index] : distanceCache)
		{
			Star& star = *stars[index];
			star.RecalculateTransformation();

			const StarPushConstants pushConstants{
				star.transformationMatrix * glm::scale(glm::vec3(STAR_SCALE)),
				glm::vec4(star.radius, star.starTemperature, STAR_NOISE_OCTAVES, 0.0f),
				glm::vec4(star.starColour, 1.0f)
			};
			cmd.PushConstants(pushConstants);
			sphereMesh->Draw(cmd);
		}
	}
}
