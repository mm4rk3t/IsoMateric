#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec3 iOffset;
layout (location = 3) in vec3 iScale;
layout (location = 4) in vec4 iColor;

uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane; // mirror passes cut away what is behind the glass

out vec4 vColor;

void main()
{
	vColor = iColor;
	vec3 world = iOffset + aPos * iScale;
	gl_ClipDistance[0] = dot(vec4(world, 1.0), clipPlane);
	gl_Position = projection * view * vec4(world, 1.0);
}
