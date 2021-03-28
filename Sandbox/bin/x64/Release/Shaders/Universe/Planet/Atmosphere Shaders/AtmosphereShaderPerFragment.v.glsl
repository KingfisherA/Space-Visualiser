#version 330

//Vertex position attribute
in vec3 vertex;
//
//Matricies for transformation
uniform mat4 transformationMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
//
//The current translation of the object
uniform vec3 translation;
// The camera's current position
uniform vec3 v3CameraPos;			
//
//The camera's position with the appropiate transformations applied
out vec3 v3CameraPosTransformed;
//The camera's height squared
out float fCameraHeight2;
//Th camera's height
out float fCameraHeight;
//The 3D position of the vertex in world space without any translation
out vec3 v3Pos;

void main(void)
{
   v3CameraPosTransformed = v3CameraPos - translation;
  //Calculate the camera height (the length of the std::vector) using the dot product but leave it squared to prevent
  //unnessesary square rooting
   fCameraHeight2 = dot(v3CameraPosTransformed, v3CameraPosTransformed);
  //Now perform the square root of fCameraHeight2 to get the actual length of the std::vector
   fCameraHeight = sqrt(fCameraHeight2);

    vec4 worldPosition = transformationMatrix * vec4(vertex, 1.0f);
	v3Pos = worldPosition.xyz - translation;

	gl_Position = projectionMatrix * viewMatrix * worldPosition;
}