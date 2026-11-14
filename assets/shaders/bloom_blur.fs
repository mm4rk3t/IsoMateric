#version 330 core

// Bloom step 2: separable 9-tap Gaussian, run horizontally then vertically.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D source;
uniform vec3 direction; // xy: one texel step along the blur axis

const float weights[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec3 sum = texture(source, vUV).rgb * weights[0];
	for (int i = 1; i < 5; i++)
	{
		vec2 o = direction.xy * float(i) * 1.5;
		sum += texture(source, vUV + o).rgb * weights[i];
		sum += texture(source, vUV - o).rgb * weights[i];
	}
	FragColor = vec4(sum, 1.0);
}
