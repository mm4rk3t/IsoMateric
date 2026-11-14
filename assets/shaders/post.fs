#version 330 core

// Pixelizer: the scene was rendered at (window / pixelSize) and is sampled here with
// nearest filtering, so every scene texel becomes a pixelSize x pixelSize block.
// Optional posterize with 4x4 ordered (Bayer) dithering for a limited-palette look.

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D scene;
uniform sampler2D sceneDepth;
uniform bool outlines;
uniform vec3 outlineColor;
uniform float outlineWidth;   // in scene texels
uniform float nearPlane;
uniform float farPlane;
uniform bool perspective;

float linearDepth(vec2 uv)
{
	float d = texture(sceneDepth, uv).r;
	if (perspective)
	{
		float z = d * 2.0 - 1.0;
		return 2.0 * nearPlane * farPlane / (farPlane + nearPlane - z * (farPlane - nearPlane));
	}
	return nearPlane + d * (farPlane - nearPlane);
}

uniform sampler2D sceneNormal;

// Lines where the surface normal turns sharply (creases, block edges against other
// faces) or the depth jumps (silhouettes against the sky or things behind). Coplanar
// runs of blocks stay clean, as in an ink drawing.
float edge(vec2 uv)
{
	vec2 t = outlineWidth / vec2(textureSize(sceneDepth, 0));
	vec2 offsets[4] = vec2[4](vec2(t.x, 0.0), vec2(-t.x, 0.0), vec2(0.0, t.y), vec2(0.0, -t.y));
	vec4 nc = texture(sceneNormal, uv);
	float dc = linearDepth(uv);
	float e = 0.0;
	for (int i = 0; i < 4; i++)
	{
		vec4 n = texture(sceneNormal, uv + offsets[i]);
		float d = linearDepth(uv + offsets[i]);
		if (abs(n.a - nc.a) > 0.5)
			e = 1.0; // geometry next to sky/particles
		else if (nc.a > 0.5 && dot(nc.xyz * 2.0 - 1.0, n.xyz * 2.0 - 1.0) < 0.6)
			e = 1.0; // crease
		else if (d - dc > 0.5 * (1.0 + dc * 0.02))
			e = 1.0; // this pixel is in front of something farther away
	}
	return e;
}

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
	if (outlines)
		c = mix(c, outlineColor, edge(vUV));
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
