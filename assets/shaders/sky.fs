#version 330 core

// Sky backdrop drawn behind the scene. The camera is orthographic (every pixel looks the
// same way), so a cubemap would be one flat colour; instead each pixel gets a "virtual"
// direction from a wide perspective view that turns with the camera's yaw. Panning adds
// a small offset for parallax.
//   mode 1: atmosphere - gradient, sun/moon disc, clouds above, a sea of clouds below,
//           stars and comets at night (driven by the day/night cycle)
//   mode 3: overcast   - a low, heavy cloud deck drifting with the wind, the sun or moon
//           only a diffuse glow through it; dark gothic weather
//   mode 2: galaxy     - Milky Way band with dust lanes, nebulae, a distant spiral galaxy,
//           three star layers and comets

in vec2 vUV;
out vec4 FragColor;

uniform int mode;
uniform vec3 viewOrigin;     // xy: scene area origin in the framebuffer
uniform vec3 viewSize;       // xy: scene area size
uniform vec3 camForward;     // horizontal forward of the camera
uniform vec3 camRight;
uniform float time;
uniform vec3 focus;
uniform float brightness;
uniform vec3 skyTop;
uniform vec3 skyHorizon;
uniform float nightAmount;   // 0 day .. 1 night (stars, comets)
uniform float bloodMoon;
uniform vec3 celestialDir;   // towards the sun (day) or moon (night)
uniform vec3 celestialColor;
uniform bool comets;
uniform bool realView;       // perspective camera: use the actual view rays
uniform vec3 viewForward;    // full 3D forward / up of the perspective camera
uniform vec3 viewUp;
uniform float tanHalfFov;
uniform vec3 wind;
uniform bool mirrored;        // drawn inside a mirror: the cross products below flip sign           // (x, 0, z): the overcast deck drifts with it

const float PI = 3.14159265;

float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p)
{
	vec3 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x), mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x), mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y),
	           f.z);
}

float fbm(vec3 p)
{
	float s = 0.0, a = 0.5;
	for (int i = 0; i < 5; i++)
	{
		s += a * noise3(p);
		p = p * 2.03 + vec3(1.7, 9.2, 3.1);
		a *= 0.5;
	}
	return s;
}

vec3 starLayer(vec3 d, float scale, float density)
{
	vec3 p = d * scale;
	vec3 cell = floor(p);
	float h = hash13(cell);
	if (h > density)
		return vec3(0.0);
	vec3 c = cell + 0.5 + (vec3(hash13(cell + 1.7), hash13(cell + 4.3), hash13(cell + 7.9)) - 0.5) * 0.7;
	float twinkle = 0.65 + 0.35 * sin(time * (1.5 + h * 40.0) + h * 80.0);
	float s = smoothstep(0.18, 0.0, length(p - c)) * twinkle;
	vec3 tint = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.85, 0.7), fract(h * 37.0));
	return tint * s * (0.6 + fract(h * 91.0) * 0.9);
}

vec3 stars(vec3 d)
{
	return starLayer(d, 70.0, 0.05) + starLayer(d, 160.0, 0.04) * 0.8 + starLayer(d, 320.0, 0.03) * 0.6;
}

vec3 cometField(vec3 d)
{
	vec3 col = vec3(0.0);
	for (int k = 0; k < 3; k++)
	{
		float period = 17.0 + float(k) * 11.0;
		float tt = time + float(k) * 7.3;
		float cycle = floor(tt / period);
		float phase = fract(tt / period);
		const float life = 0.22;
		if (phase > life)
			continue;
		float t = phase / life;
		vec3 seed = vec3(cycle, float(k), 3.0);
		vec3 a = normalize(vec3(hash13(seed) - 0.5, hash13(seed + 1.3) * 0.5 + 0.15, hash13(seed + 2.7) - 0.5));
		vec3 b = normalize(a + normalize(vec3(hash13(seed + 5.1) - 0.5, -0.25, hash13(seed + 8.4) - 0.5)) * 0.8);
		vec3 head = normalize(mix(a, b, t));
		vec3 tail = normalize(mix(a, b, max(t - 0.3, 0.0)));
		vec3 ab = head - tail;
		float u = clamp(dot(d - tail, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
		float dist = length(d - (tail + ab * u));
		float fade = sin(t * PI);
		col += vec3(0.75, 0.9, 1.0) * smoothstep(0.006, 0.0, dist) * u * u * fade * 1.6;
		col += vec3(1.0) * smoothstep(0.012, 0.0, length(d - head)) * fade * 2.5;
	}
	return col;
}

vec3 atmosphere(vec3 d, vec2 pan)
{
	vec3 col;
	if (d.y >= 0.0)
	{
		col = mix(skyHorizon, skyTop, pow(d.y, 0.45));
		col += stars(d) * nightAmount * smoothstep(0.0, 0.2, d.y);
		if (comets)
			col += cometField(d) * nightAmount;
	}
	else
	{
		col = skyHorizon;
	}

	// sun or moon disc with a glow
	float c = dot(d, normalize(celestialDir));
	vec3 discColor = nightAmount > 0.5 ? mix(vec3(0.9, 0.92, 1.0), vec3(1.0, 0.15, 0.08), bloodMoon) : celestialColor * 1.4 + 0.2;
	float disc = smoothstep(nightAmount > 0.5 ? 0.9985 : 0.9991, nightAmount > 0.5 ? 0.9990 : 0.9995, c);
	float glow = pow(max(c, 0.0), 180.0) * 0.6 + pow(max(c, 0.0), 10.0) * 0.12;
	col += discColor * (disc * 2.0 + glow);

	if (d.y >= 0.0)
	{
		// two cloud layers on a plane above, drifting and shifting with the camera pan
		vec2 uv = d.xz / (d.y + 0.08);
		float far = fbm(vec3(uv * 0.5 + time * vec2(0.010, 0.004) + pan * 0.02, 0.0));
		float near = fbm(vec3(uv * 0.9 + time * vec2(0.018, 0.006) + pan * 0.05, 4.0));
		float cover = smoothstep(0.52, 0.72, far) * 0.6 + smoothstep(0.55, 0.78, near) * 0.7;
		cover *= smoothstep(0.0, 0.12, d.y);
		vec3 cloud = mix(skyHorizon * 1.15, skyHorizon * 0.8 + celestialColor * 0.45, near);
		col = mix(col, cloud, clamp(cover, 0.0, 0.85));
	}
	else
	{
		// a sea of clouds below the level
		vec2 uv = d.xz / (-d.y + 0.03) * 0.5 + pan * 0.08;
		float sea = fbm(vec3(uv * 0.8 + time * vec2(0.02, 0.01), 1.0));
		vec3 lit = skyHorizon * 1.25 + celestialColor * 0.25;
		vec3 shade = skyHorizon * 0.55 + skyTop * 0.15;
		vec3 seaColor = mix(shade, lit, smoothstep(0.35, 0.72, sea));
		col = mix(skyHorizon, seaColor, smoothstep(0.0, 0.18, -d.y));
	}
	return col;
}

vec3 galaxy(vec3 d)
{
	// slow drift
	float a = time * 0.004;
	d = vec3(d.x * cos(a) - d.z * sin(a), d.y, d.x * sin(a) + d.z * cos(a));

	vec3 col = vec3(0.012, 0.008, 0.028);

	// Milky Way band along a tilted great circle
	vec3 bandNormal = normalize(vec3(0.35, 1.0, 0.45));
	float b = dot(d, bandNormal);
	float band = exp(-b * b / 0.07);
	float n1 = fbm(d * 3.0);
	float n2 = fbm(d * 7.0 + 5.0);
	col += band * (0.55 * n1 + 0.45 * n2) * vec3(0.32, 0.24, 0.55);
	col += band * band * pow(n2, 3.0) * vec3(0.95, 0.65, 0.42) * 0.9;
	col *= 1.0 - band * smoothstep(0.55, 0.78, fbm(d * 9.0 + 3.0)) * 0.75; // dust lanes

	// coloured nebulae
	col += pow(max(fbm(d * 2.2 + 11.0) - 0.45, 0.0), 2.0) * vec3(0.9, 0.2, 0.6) * 2.2;
	col += pow(max(fbm(d * 2.6 - 7.0) - 0.48, 0.0), 2.0) * vec3(0.1, 0.5, 0.95) * 2.4;

	// a distant, tilted spiral galaxy
	vec3 g = normalize(vec3(-0.55, 0.35, 0.75));
	vec3 tangent = normalize(cross(g, vec3(0.0, 1.0, 0.0)));
	vec3 bitangent = cross(tangent, g);
	float facing = dot(d, g);
	if (facing > 0.0)
	{
		vec2 q = vec2(dot(d, tangent), dot(d, bitangent)) / 0.20;
		q.y /= 0.5; // inclined disc
		float r = length(q);
		float th = atan(q.y, q.x);
		float arms = pow(0.5 + 0.5 * cos(2.0 * th - log(r + 1e-3) * 4.0 + time * 0.02), 4.0);
		float disk = exp(-r * 3.0) * (0.25 + arms * 0.9) * smoothstep(1.3, 0.0, r);
		float core = exp(-r * r * 40.0);
		col += (disk * vec3(0.55, 0.5, 0.95) + core * vec3(1.0, 0.85, 0.6)) * 1.3;
	}

	col += stars(d) * (1.0 + band * 0.8);
	if (comets)
		col += cometField(d);
	return col;
}

vec3 overcast(vec3 d, vec2 pan)
{
	vec3 low = mix(skyHorizon, skyTop, 0.5) * 0.6;   // underside of the deck
	vec3 col;
	vec2 drift = time * (wind.xz * 0.012 + vec2(0.004, 0.002));
	if (d.y >= -0.02)
	{
		vec2 uv = d.xz / (max(d.y, 0.0) + 0.12) + pan * 0.03;
		float big = fbm(vec3(uv * 0.35 + drift, 2.0));
		float rag = fbm(vec3(uv * 1.1 + drift * 1.8, 7.0));
		float thick = clamp(big * 1.2 + rag * 0.5 - 0.35, 0.0, 1.0);
		vec3 thin = mix(skyHorizon, skyTop, pow(max(d.y, 0.0), 0.6)) * 0.85;
		col = mix(thin, low * (0.7 + 0.5 * rag), smoothstep(0.15, 0.7, thick));

		// the sun or moon only as a soft bright patch where the deck is thinner
		float c = max(dot(d, normalize(celestialDir)), 0.0);
		vec3 lightColor = nightAmount > 0.5 ? mix(vec3(0.75, 0.8, 0.95), vec3(1.0, 0.15, 0.08), bloodMoon) : celestialColor + 0.15;
		float glow = pow(c, 40.0) * 0.9 + pow(c, 6.0) * 0.18;
		col += lightColor * glow * (1.0 - 0.7 * thick);
		// silver lining on the ragged edges facing the light
		col += lightColor * pow(c, 12.0) * smoothstep(0.35, 0.5, rag) * (1.0 - smoothstep(0.5, 0.65, rag)) * 0.5;

		// the odd star through a gap at night
		col += stars(d) * nightAmount * (1.0 - smoothstep(0.05, 0.35, thick)) * smoothstep(0.0, 0.2, d.y);
	}
	else
	{
		col = skyHorizon * 0.7;
	}
	// murky haze towards the horizon and below
	float haze = 1.0 - smoothstep(-0.25, 0.25, d.y);
	col = mix(col, skyHorizon * 0.65, haze);
	return col;
}

void main()
{
	// virtual perspective view: horizontal forward pitched up a little, 80 degree fov
	vec2 uv = (gl_FragCoord.xy - viewOrigin.xy) / viewSize.xy;
	vec2 ndc = uv * 2.0 - 1.0;
	float aspect = viewSize.x / viewSize.y;
	float pitch = radians(6.0);
	vec3 fwd = normalize(camForward * cos(pitch) + vec3(0.0, sin(pitch), 0.0));
	vec3 up = normalize(cross(camRight, fwd));
	if (mirrored)
		up = -up;
	float t = tan(radians(40.0));
	vec2 pan = vec2(dot(focus, camRight), dot(focus, camForward));
	vec3 d = normalize(fwd + ndc.x * t * aspect * camRight + ndc.y * t * up
	                   + camRight * pan.x * 0.003 + up * pan.y * 0.002);
	if (realView)
	{
		vec3 r = normalize(cross(viewForward, viewUp));
		if (mirrored)
			r = -r;
		d = normalize(viewForward + ndc.x * tanHalfFov * aspect * r + ndc.y * tanHalfFov * viewUp);
	}

	vec3 col = mode == 2 ? galaxy(d) : mode == 3 ? overcast(d, pan) : atmosphere(d, pan);
	FragColor = vec4(col * brightness, 1.0);
}
