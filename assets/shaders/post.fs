#version 330 core

// Pixelizer: the scene was rendered at (window / pixelSize) and is sampled here with
// nearest filtering, so every scene texel becomes a pixelSize x pixelSize block.
// Optional posterize with 4x4 ordered (Bayer) dithering for a limited-palette look.

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D scene;
uniform int colorLevels;  // 0 = off, otherwise levels per channel
uniform bool dither;

const float bayer[16] = float[16](
	 0.0,  8.0,  2.0, 10.0,
	12.0,  4.0, 14.0,  6.0,
	 3.0, 11.0,  1.0,  9.0,
	15.0,  7.0, 13.0,  5.0);

void main()
{
	vec3 c = texture(scene, vUV).rgb;
	if (colorLevels > 0)
	{
		float levels = float(colorLevels - 1);
		float offset = 0.0;
		if (dither)
		{
			// one dither cell per *scene* texel, so the pattern is pixel-art sized too
			ivec2 texel = ivec2(vUV * vec2(textureSize(scene, 0))) & 3;
			offset = bayer[texel.y * 4 + texel.x] / 16.0 - 0.5;
		}
		c = floor(c * levels + 0.5 + offset) / levels;
	}
	FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
