#version 330 core

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 texCoords;
//
out vec2 textureCoords;
//
uniform mat4 depthMVP;
//
void main()
{
    textureCoords = texCoords;
    gl_Position = depthMVP * vec4(position, 1.0f);
}