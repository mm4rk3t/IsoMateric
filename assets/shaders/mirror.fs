#version 330 core

// The scene was already rendered mirrored across this quad's plane, with the same
// projection, so the reflection for this pixel sits at the same screen position.
in vec2 vUV;
out vec4 FragColor;
layout (location = 1) out vec4 NormalOut;

uniform sampler2D reflection;
uniform vec3 viewOrigin;  // xy: scene area origin in the framebuffer
uniform vec3 viewSize;    // xy: scene area size
uniform vec3 normal;
uniform vec3 tint;
uniform float tarnish;    // 0 clean .. 1 old silvering

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}

void main()
{
	vec2 uv = (gl_FragCoord.xy - viewOrigin.xy) / viewSize.xy;
	vec3 c = texture(reflection, uv).rgb * tint;

	// a faint sheen so a mirror reads as glass even when it reflects something dark
	c += vec3(0.02, 0.022, 0.026);

	if (tarnish > 0.0)
	{
		// old silvering: dark blotches creeping in from the edges, a little grime
		vec2 q = vUV * vec2(6.0, 9.0);
		float blotch = noise(q) * 0.6 + noise(q * 2.7 + 3.1) * 0.4;
		float edge = 1.0 - smoothstep(0.0, 0.22, min(min(vUV.x, 1.0 - vUV.x), min(vUV.y, 1.0 - vUV.y)));
		float rot = smoothstep(0.55, 0.85, blotch + edge * 0.45) * tarnish;
		c = mix(c, c * 0.25 + vec3(0.05, 0.045, 0.03), rot);
		c *= 1.0 - tarnish * 0.15 * noise(vUV * 40.0);
	}

	FragColor = vec4(c, 1.0);
	NormalOut = vec4(normalize(normal) * 0.5 + 0.5, 1.0);
}
