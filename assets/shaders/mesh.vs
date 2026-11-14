#version 330 core

#define MAX_JOINTS 32

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
uniform mat4 joints[MAX_JOINTS];
uniform bool skinned;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vLightSpacePos;

void main()
{
	mat4 skin = mat4(1.0);
	if (skinned)
		skin = aWeights.x * joints[int(aJoints.x)] + aWeights.y * joints[int(aJoints.y)]
		     + aWeights.z * joints[int(aJoints.z)] + aWeights.w * joints[int(aJoints.w)];
	mat4 m = model * skin;
	vec4 world = m * vec4(aPos, 1.0);
	vWorldPos = world.xyz;
	vNormal = mat3(m) * aNormal; // fine for uniform scale, which is all the engine uses
	vUV = aUV;
	vLightSpacePos = lightSpace * world;
	gl_Position = projection * view * world;
}
