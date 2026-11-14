#version 330 core

layout (location = 0) in vec3 aPos; // unit sphere, position == normal

uniform mat4 view;
uniform mat4 projection;
uniform vec3 center;
uniform float radius;

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
	vWorldPos = center + aPos * radius;
	vNormal = aPos;
	gl_Position = projection * view * vec4(vWorldPos, 1.0);
}
