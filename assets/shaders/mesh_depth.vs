#version 330 core

#define MAX_JOINTS 32

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 lightSpace;
uniform mat4 joints[MAX_JOINTS];
uniform bool skinned;

void main()
{
	mat4 skin = mat4(1.0);
	if (skinned)
		skin = aWeights.x * joints[int(aJoints.x)] + aWeights.y * joints[int(aJoints.y)]
		     + aWeights.z * joints[int(aJoints.z)] + aWeights.w * joints[int(aJoints.w)];
	gl_Position = lightSpace * model * skin * vec4(aPos, 1.0);
}
