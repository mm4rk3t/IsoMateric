#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vLightSpacePos;

out vec4 FragColor;
layout (location = 1) out vec4 NormalOut; // for the outline pass

uniform vec4 baseColor;
uniform bool hasTexture;
uniform sampler2D albedo;

#include "lighting.glsl"

void main()
{
	vec4 base = baseColor;
	if (hasTexture)
		base *= texture(albedo, vUV);
	vec3 lit = shade(base.rgb, vWorldPos, vNormal, vLightSpacePos);
	NormalOut = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
	FragColor = vec4(applyFog(lit, vWorldPos), base.a);
}
