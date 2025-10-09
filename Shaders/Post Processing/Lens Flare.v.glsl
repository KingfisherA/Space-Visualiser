#version 150

in vec2 position;
//
out vec2 Texcoord;
//
uniform vec4 transform;

void main(void) 
{
    Texcoord = position + vec2(0.5,0.5);

   vec2 screenPosition = position * transform.zw + transform.xy;
   screenPosition.x = screenPosition.x * 2.0 - 1.0;
   screenPosition.y = screenPosition.y * -2.0 + 1.0;

    gl_Position = vec4(screenPosition, 0.0, 1.0);
}