#version 410
#extension GL_ARB_shading_language_420pack : require

// The pixel's position in normalised device coordinates, to cast its ray from
layout(location = 0) out vec2 vNdc;

// Full screen triangle generated from the vertex index; no vertex buffers needed
void main()
{
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    vNdc = position;
    gl_Position = vec4(position, 0.0, 1.0);
}
