#version 330 core

// Bloom step 1: keep only bright pixels (soft knee), read from a blurrier mip so the
// downsample does not alias.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D source;
uniform float threshold;

void main()
{
	vec3 c = textureLod(source, vUV, 1.5).rgb;
	float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
	float keep = smoothstep(threshold, threshold + 0.25, luma);
	FragColor = vec4(c * keep, 1.0);
}
