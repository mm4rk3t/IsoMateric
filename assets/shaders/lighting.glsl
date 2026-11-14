// Shared lighting for world.fs and mesh.fs (pulled in with #include).

#define MAX_LIGHTS 32

layout (std140) uniform Lights
{
	vec4 lightPositionRadius[MAX_LIGHTS];
	vec4 lightColorIntensity[MAX_LIGHTS];
};

uniform int numLights;
uniform vec3 viewDir;   // towards the camera (orthographic, so constant)
uniform vec3 sunDir;    // direction the sunlight travels
uniform vec3 sunColor;
uniform float ambient;
uniform bool shadowsEnabled;
uniform sampler2D shadowMap;

// weather
uniform float wetness;       // 0 dry .. 1 soaked: darker surfaces, sharper highlights
uniform bool fogEnabled;
uniform vec3 fogColor;
uniform float fogDensity;    // ground mist strength
uniform float fogHeight;     // how high the mist reaches above fogBase
uniform float fogBase;       // ground level of the mist
uniform float fogDistance;   // extra haze per unit of distance from fogFocus
uniform vec3 fogFocus;       // usually the camera focus
uniform float time;
float hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

// low-lying mist that drifts slowly, thicker near the ground, plus distance haze
vec3 applyFog(vec3 color, vec3 p)
{
	if (!fogEnabled)
		return color;
	float layer = exp(-max(p.y - fogBase, 0.0) / max(fogHeight, 0.01));
	float drift = valueNoise(p.xz * 0.35 + vec2(time * 0.25, time * 0.11)) * 0.6
	            + valueNoise(p.xz * 0.9 - vec2(time * 0.17, -time * 0.21)) * 0.4;
	float mist = fogDensity * layer * (0.55 + 0.9 * drift);
	float haze = length(p.xz - fogFocus.xz) * fogDistance;
	float f = 1.0 - exp(-(mist + haze));
	return mix(color, fogColor, clamp(f, 0.0, 0.95));
}

float sunShadow(vec4 lightSpacePos, vec3 n, vec3 l)
{
	vec3 p = lightSpacePos.xyz / lightSpacePos.w * 0.5 + 0.5;
	if (p.z > 1.0)
		return 0.0;
	// tiny constant bias in depth units; the real fix for acne is the normal offset
	// applied in the vertex shader, a large bias here leaks light under walls
	float bias = 0.0002;
	vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
	float shadow = 0.0;
	for (int x = -1; x <= 1; x++)
		for (int y = -1; y <= 1; y++)
			shadow += (p.z - bias > texture(shadowMap, p.xy + vec2(x, y) * texel).r) ? 1.0 : 0.0;
	return shadow / 9.0;
}

vec3 shade(vec3 base, vec3 worldPos, vec3 normal, vec4 lightSpacePos)
{
	vec3 n = normalize(normal);
	vec3 v = normalize(viewDir);
	base *= 1.0 - 0.35 * wetness; // wet materials look darker
	float shininess = mix(32.0, 96.0, wetness);
	float specAmount = mix(0.3, 1.2, wetness);

	// sun (Lambert)
	vec3 l = normalize(-sunDir);
	float sunAmount = max(dot(n, l), 0.0);
	float shadow = shadowsEnabled ? sunShadow(lightSpacePos, n, l) : 0.0;
	vec3 color = base * ambient + base * sunColor * sunAmount * (1.0 - shadow);
	color += sunColor * pow(max(dot(n, normalize(l + v)), 0.0), shininess) * wetness * (1.0 - shadow) * 0.6;

	// point lights (Blinn-Phong with smooth range falloff)
	for (int i = 0; i < numLights; i++)
	{
		vec3 toLight = lightPositionRadius[i].xyz - worldPos;
		float dist = length(toLight);
		float falloff = clamp(1.0 - dist / lightPositionRadius[i].w, 0.0, 1.0);
		falloff *= falloff;
		if (falloff <= 0.0)
			continue;

		vec3 ld = toLight / max(dist, 1e-4);
		float diffuse = max(dot(n, ld), 0.0);
		float specular = pow(max(dot(n, normalize(ld + v)), 0.0), shininess) * specAmount;
		vec3 light = lightColorIntensity[i].rgb * lightColorIntensity[i].a * falloff;
		color += (base * diffuse + specular) * light;
	}
	return color;
}
