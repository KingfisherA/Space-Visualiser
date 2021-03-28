varying vec4 Texcoord;
uniform sampler2D fbo_texture;
uniform vec2 texOffset;

const float kernel[9] = float[](
    0.00481007202f,
    0.0286864862f,
    0.102712765f,
    0.220796734f,
    0.284958780f,
    0.220796734f,
    0.102712765f,
    0.0286864862f,
    0.00481007202f
);
void main(void)
{
   vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
   vec2 tex = vec2(Texcoord.x, Texcoord.y);
    for (int i = -4; i <= 4; i++)
	{
  vec4 value;
   value = texture2D( fbo_texture,  vec2(tex + i * texOffset));
   sum += vec4(value* kernel[i + 4]);
   }
		
   gl_FragColor = sum;
}
