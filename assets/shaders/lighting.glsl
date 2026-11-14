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

	// sun (Lambert)
	vec3 l = normalize(-sunDir);
	float sunAmount = max(dot(n, l), 0.0);
	float shadow = shadowsEnabled ? sunShadow(lightSpacePos, n, l) : 0.0;
	vec3 color = base * ambient + base * sunColor * sunAmount * (1.0 - shadow);

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
		float specular = pow(max(dot(n, normalize(ld + v)), 0.0), 32.0) * 0.3;
		vec3 light = lightColorIntensity[i].rgb * lightColorIntensity[i].a * falloff;
		color += (base * diffuse + specular) * light;
	}
	return color;
}
