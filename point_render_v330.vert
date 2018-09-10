#version 330

uniform mat4 MVP;

layout (location = 0) in vec3 vertex;

void main()
{
	gl_Position = MVP * vec4(vertex, 1.0);
}
