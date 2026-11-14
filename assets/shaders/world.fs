#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vLightSpacePos;

out vec4 FragColor;

#include "lighting.glsl"

void main()
{
	FragColor = vec4(shade(vColor.rgb, vWorldPos, vNormal, vLightSpacePos), vColor.a);
}
