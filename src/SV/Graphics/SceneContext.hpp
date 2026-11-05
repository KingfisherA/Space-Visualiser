#pragma once
#include <glm/glm.hpp>

#include "Dahlia/Graphics/BindGroup.h"
#include "Dahlia/Graphics/GraphicsTypes.h"
#include "Dahlia/Graphics/Texture.h"
#include "Dahlia/Utilities/Memory.h"

namespace SV
{
	// Mirrors the FrameUniforms block in Shaders/Include/Frame.glsl (std140)
	struct FrameUniforms
	{
		glm::mat4 viewProjection{ 1.0f };
		glm::mat4 view{ 1.0f };
		glm::mat4 projection{ 1.0f };
		glm::vec4 cameraPosition{ 0.0f };
		glm::vec4 cameraCentre{ 0.0f };
		glm::vec4 sun{ 0.0f };     // xyz = the Sun's position (world space), w = the Sun's radius
		glm::vec4 times{ 0.0f };   // x = planet animation time, y = star animation time
		glm::vec4 display{ 0.0f }; // x = exposure
	};

	// Resources shared by every scene renderer. Scene pipelines use set 0 for the frame uniforms.
	struct SceneContext
	{
		// Attachments of the viewport framebuffer the scene is drawn into
		DH::RenderTargetLayout targetLayout;

		sRef<DH::BindGroupLayout> frameLayout;
		sRef<DH::BindGroup> frameGroup;

		// Planet maps: trilinear, wrapping
		sRef<DH::Sampler> mapSampler;
		// Planet rings: trilinear, transparent outside the ring texture
		sRef<DH::Sampler> ringSampler;
		// Precomputed atmosphere textures: bilinear, clamped
		sRef<DH::Sampler> clampSampler;

		// 1x1 opaque black, bound in place of missing or still-loading maps
		sRef<DH::Texture2D> blackTexture;
	};
}
