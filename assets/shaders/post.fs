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

// colour grading: desaturate, add contrast, tint shadows and highlights apart
uniform bool grading;
uniform float saturation;      // 1 = unchanged, 0 = greyscale
uniform float contrast;        // 1 = unchanged
uniform vec3 shadowTint;       // multiplied into the darks
uniform vec3 highlightTint;    // multiplied into the lights
uniform float vignette;        // 0 off .. 1 heavy darkening towards the corners
uniform float grain;           // 0 off .. film grain amount
uniform float time;

vec3 grade(vec3 c)
{
	float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
	c = mix(vec3(luma), c, saturation);
	c = (c - 0.5) * contrast + 0.5;
	float l = clamp(dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
	vec3 tint = mix(shadowTint, highlightTint, smoothstep(0.15, 0.75, l));
	return max(c * tint, 0.0);
}

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

// God rays: march each pixel's view ray through the level volume and count how much
// of it the sun (or moon) actually reaches, using the shadow map. Walls, the trunk and
// branches cut dark wedges out of the haze, which reads as shafts of light.
uniform bool godRays;
uniform mat4 invViewProj;
uniform mat4 lightSpace;
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
uniform vec3 sunDir;        // direction the light travels
uniform vec3 sunColor;
uniform vec3 raysMin, raysMax;
uniform float rayBase;      // haze is densest at this height ...
uniform float rayFalloff;   // ... and thins out over this many blocks above it
uniform float godRayStrength;
uniform int godRaySteps;

const float bayerRays[16] = float[16](
	 0.0,  8.0,  2.0, 10.0,
	12.0,  4.0, 14.0,  6.0,
	 3.0, 11.0,  1.0,  9.0,
	15.0,  7.0, 13.0,  5.0);

vec3 lightShafts(vec2 uv)
{
	float depth = texture(sceneDepth, uv).r;
	vec4 a = invViewProj * vec4(uv * 2.0 - 1.0, -1.0, 1.0);
	vec4 b = invViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec3 from = a.xyz / a.w, to = b.xyz / b.w;
	vec3 rd = to - from;
	float len = length(rd);
	rd /= len;

	// clip the ray to the level volume
	vec3 inv = 1.0 / rd;
	vec3 t0 = (raysMin - from) * inv, t1 = (raysMax - from) * inv;
	vec3 tlo = min(t0, t1), thi = max(t0, t1);
	float tn = max(max(tlo.x, tlo.y), max(tlo.z, 0.0));
	float tf = min(min(thi.x, thi.y), min(thi.z, len));
	if (tf <= tn)
		return vec3(0.0);

	float stepLen = (tf - tn) / float(godRaySteps);
	ivec2 cell = ivec2(gl_FragCoord.xy) & 3;
	float jitter = (bayerRays[cell.y * 4 + cell.x] + 0.5) / 16.0; // hides the banding between steps
	float scatter = 0.0;
	for (int i = 0; i < 64; i++)
	{
		if (i >= godRaySteps)
			break;
		vec3 p = from + rd * (tn + (float(i) + jitter) * stepLen);
		float lit = 1.0;
		if (shadowsEnabled)
		{
			vec4 ls = lightSpace * vec4(p, 1.0);
			vec3 s = ls.xyz / ls.w * 0.5 + 0.5;
			if (all(greaterThan(s.xy, vec2(0.0))) && all(lessThan(s.xy, vec2(1.0))) && s.z < 1.0)
				lit = s.z - 0.003 <= texture(shadowMap, s.xy).r ? 1.0 : 0.0;
		}
		float density = exp(-max(p.y - rayBase, 0.0) / rayFalloff);
		// fade out towards the sides of the volume, so it never shows a hard edge
		vec3 inside = min(p - raysMin, raysMax - p);
		density *= smoothstep(0.0, 3.0, min(inside.x, inside.z)) * smoothstep(0.0, 1.5, inside.y);
		scatter += lit * density * stepLen;
	}
	// brighter when looking into the light (forward scattering)
	float facing = max(dot(rd, -normalize(sunDir)), 0.0);
	float phase = 0.45 + 0.9 * pow(facing, 4.0);
	return sunColor * scatter * phase * godRayStrength * 0.1;
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
	if (godRays)
		c += lightShafts(vUV);
	if (grading)
		c = grade(c);
	if (vignette > 0.0)
	{
		vec2 v = vUV - 0.5;
		c *= 1.0 - vignette * smoothstep(0.25, 0.85, length(v * vec2(1.15, 1.0)) * 1.25);
	}
	if (grain > 0.0)
	{
		// one grain cell per scene texel, re-rolled about 24 times a second
		vec2 texel = floor(vUV * vec2(textureSize(scene, 0)));
		float n = hash12(texel + floor(time * 24.0) * vec2(17.0, 59.0)) - 0.5;
		float luma = dot(c, vec3(0.333));
		c += n * grain * (0.35 + 0.65 * (1.0 - luma)); // grain shows most in the darks
	}
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
