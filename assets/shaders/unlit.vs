#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec3 iOffset;
layout (location = 3) in vec3 iScale;
layout (location = 4) in vec4 iColor;

uniform mat4 view;
uniform mat4 projection;

out vec4 vColor;

void main()
{
	vColor = iColor;
	gl_Position = projection * view * vec4(iOffset + aPos * iScale, 1.0);
}
