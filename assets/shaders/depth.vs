#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec3 iOffset;
layout (location = 3) in vec3 iScale;

uniform mat4 lightSpace;

void main()
{
	gl_Position = lightSpace * vec4(iOffset + aPos * iScale, 1.0);
}
