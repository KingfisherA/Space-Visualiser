#version 410
#extension GL_ARB_shading_language_420pack : require

// Full screen triangle generated from the vertex index; no vertex buffers needed.
void main()
{
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
