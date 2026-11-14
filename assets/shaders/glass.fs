#version 330 core

// Glass sphere: screen-space refraction of the frame drawn so far (grab texture),
// Beer-Lambert absorption over the thickness, Schlick Fresnel reflection of a simple
// sky, sun and point-light highlights, per-channel IOR for dispersion and blurrier
// mip levels for rough (frosted) glass.

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;

#include "lighting.glsl"

uniform mat4 view;
uniform mat4 projection;
uniform sampler2D grab;
uniform vec3 viewOrigin;  // xy: where the scene starts in the framebuffer
uniform vec3 viewSize;    // xy: size of the scene area / grab texture

uniform vec3 center;
uniform float radius;
uniform vec3 tint;
uniform float ior;
uniform float roughness;
uniform float absorption;
uniform float dispersion;

vec3 sky(vec3 dir)
{
	vec3 zenith = vec3(0.30, 0.42, 0.62);
	vec3 horizon = vec3(0.75, 0.78, 0.80);
	vec3 ground = vec3(0.30, 0.32, 0.26);
	return dir.y >= 0.0 ? mix(horizon, zenith, pow(dir.y, 0.6)) : mix(horizon, ground, pow(-dir.y, 0.4));
}

void main()
{
	vec3 n = normalize(vNormal);
	vec3 v = normalize(viewDir);
	float ndotv = clamp(dot(n, v), 0.0, 1.0);

	// thickness: the chord the view ray cuts through the sphere at this pixel
	vec3 c = vWorldPos - center;
	vec3 perp = c - dot(c, v) * v;
	float thickness = 2.0 * sqrt(max(radius * radius - dot(perp, perp), 0.0));

	// refraction: shift the background lookup against the view-space normal, scaled by
	// the sphere size and IOR. Strongest at the rim and flipping the image like a ball
	// lens would. scale converts world units to UV.
	vec2 uv = (gl_FragCoord.xy - viewOrigin.xy) / viewSize.xy;
	vec2 nView = (mat3(view) * n).xy;
	vec2 scale = vec2(projection[0][0], projection[1][1]) * 0.5;
	float lod = roughness * 6.0;
	vec3 refracted;
	for (int ch = 0; ch < 3; ch++)
	{
		float channelIor = ior * (1.0 + (float(ch) - 1.0) * dispersion);
		float bend = 1.0 - 1.0 / max(channelIor, 1.0);
		vec2 offset = -nView * bend * radius * 2.5 * scale;
		refracted[ch] = textureLod(grab, clamp(uv + offset, 0.001, 0.999), lod)[ch];
	}

	// absorption through the thickness; a thin tint even when absorption is 0
	vec3 transmittance = exp(-(1.0 - tint) * (absorption * thickness + 0.35));
	vec3 color = refracted * transmittance;

	// Fresnel reflection of the sky, softened by roughness
	float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
	float fresnel = (f0 + (1.0 - f0) * pow(1.0 - ndotv, 5.0)) * (1.0 - 0.6 * roughness);
	vec3 r = reflect(-v, n);
	color = mix(color, sky(r), fresnel);

	// highlights: sun and point lights (Blinn-Phong, broader when rough)
	float shininess = mix(800.0, 24.0, roughness);
	float norm = (shininess + 8.0) / 64.0;
	vec3 l = normalize(-sunDir);
	vec3 highlight = sunColor * pow(max(dot(n, normalize(l + v)), 0.0), shininess) * norm;
	for (int i = 0; i < numLights; i++)
	{
		vec3 toLight = lightPositionRadius[i].xyz - vWorldPos;
		float dist = length(toLight);
		float falloff = clamp(1.0 - dist / lightPositionRadius[i].w, 0.0, 1.0);
		vec3 ld = toLight / max(dist, 1e-4);
		highlight += lightColorIntensity[i].rgb * lightColorIntensity[i].a * falloff * falloff
		           * pow(max(dot(n, normalize(ld + v)), 0.0), shininess) * norm;
	}
	color += highlight * (1.0 - 0.5 * roughness);

	// frosted glass scatters a little light of its own
	color += roughness * 0.12 * (ambient + max(dot(n, l), 0.0)) * tint;

	// the refracted background is already fogged; fog the glass surface itself too
	FragColor = vec4(applyFog(color, vWorldPos), 1.0);
}
