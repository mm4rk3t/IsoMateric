#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 iOffset;
layout (location = 3) in vec3 iScale;
layout (location = 4) in vec4 iColor;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
out vec4 vLightSpacePos;

void main()
{
	// boxes are only translated and scaled along the axes, so the normal is unchanged
	vec3 world = iOffset + aPos * iScale;
	vWorldPos = world;
	vNormal = aNormal;
	vColor = iColor;
	vLightSpacePos = lightSpace * vec4(world, 1.0);
	gl_Position = projection * view * vec4(world, 1.0);
}
