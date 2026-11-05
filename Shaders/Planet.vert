#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "PlanetPushConstants.glsl"

// The planet's sky mesh: a sphere or spheroid at the top of the atmosphere
layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vNormal;   // Planet space
layout(location = 2) out vec2 vTexCoord;

void main()
{
    vec4 positionWorld = uPlanet.transform * vec4(aVertex, 1.0);

    vWorldPosition = positionWorld.xyz;
    vNormal = aNormal;
    vTexCoord = aTexCoord;

    gl_Position = uFrame.viewProjection * positionWorld;
}
