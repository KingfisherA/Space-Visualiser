attribute vec2 v_coord;
uniform sampler2D fbo_texture;
varying vec4 Texcoord;

void main(void) {
 // gl_Position = vec4(v_coord, 0.0, 1.0);
 gl_Position = gl_ModelViewProjectionMatrix *gl_Vertex;
  Texcoord = vec4(vec3(gl_Vertex), 1.0);
}

