#version 330 core

// Instanced grass blades, bent by the wind in the vertex shader. Shares world.fs, so
// blades get the same lighting, shadows, fog, wetness and cutaway as the blocks.

layout (location = 0) in vec2 aBlade;  // x: side -1..1, y: 0 at the root .. 1 at the tip
layout (location = 2) in vec3 iBase;   // root, on top of the block
layout (location = 3) in vec4 iShape;  // height, half width, facing angle, random phase
layout (location = 4) in vec3 iColor;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
uniform vec4 clipPlane; // mirror passes cut away what is behind the glass (GL_CLIP_DISTANCE0)
uniform float shadowNormalOffset;
uniform float time;
uniform vec3 wind;         // (x, 0, z) units per second, the same wind that pushes the rain
uniform float windMax;     // wind speed at which blades lean the most
uniform vec3 pusher;       // the player's feet: blades bend away from it
uniform float pushRadius;

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
out vec4 vLightSpacePos;
out float vEmissive;

void main()
{
	float t = aBlade.y;
	float height = iShape.x;
	float phase = iShape.w;
	vec2 facing = vec2(cos(iShape.z), sin(iShape.z));
	vec2 across = vec2(-facing.y, facing.x);

	// wind: a steady lean plus gusts that roll across the field along the wind direction
	vec2 w = wind.xz;
	float speed = length(w);
	vec2 dir = speed > 0.0001 ? w / speed : vec2(1.0, 0.0);
	float strength = clamp(speed / windMax, 0.0, 1.0);
	float along = dot(iBase.xz, dir);
	float gust = 0.5 + 0.5 * sin(along * 0.8 - time * (1.2 + speed * 0.5));
	gust *= gust;
	float flutter = sin(time * (2.5 + speed * 0.9) + phase + along * 2.3);
	float lean = strength * (0.3 + 0.7 * gust) + flutter * 0.07 * (0.25 + strength);
	vec2 bend = dir * lean;

	// a little idle sway so a calm field is not frozen
	vec2 idleDir = vec2(cos(phase * 1.7), sin(phase * 1.7));
	bend += idleDir * 0.05 * sin(time * 1.1 + phase * 3.0);

	// blades part around the player
	vec2 away = iBase.xz - pusher.xz;
	float d = length(away);
	float push = (1.0 - smoothstep(0.1, pushRadius, d)) * (1.0 - step(0.6, abs(iBase.y - pusher.y)));
	if (d > 0.0001)
		bend += away / d * push * 1.1;

	float amount = length(bend);
	if (amount > 1.2)
		bend *= 1.2 / amount;
	amount = min(amount, 1.2);

	// the tip moves the most; leaning blades get shorter so they keep their length
	float curve = t * t;
	vec2 sideways = bend * curve * height;
	float up = height * t * (1.0 - 0.4 * min(amount * amount, 1.0) * t);
	float halfWidth = iShape.y * (1.0 - 0.9 * t);
	vec2 spread = across * aBlade.x * halfWidth + sideways;
	vec3 world = iBase + vec3(spread.x, up, spread.y);

	// mostly-up normal: blades shade like the ground they grow from, not like paper
	vec3 n = normalize(vec3(0.0, 1.0, 0.0) + vec3(facing.x, 0.0, facing.y) * 0.3 + vec3(bend.x, 0.0, bend.y) * 0.4 * t);

	vEmissive = 0.0;
	vWorldPos = world;
	vNormal = n;
	vColor = vec4(iColor * mix(0.55, 1.15, t), 1.0);
	vLightSpacePos = lightSpace * vec4(world + n * shadowNormalOffset, 1.0);
	gl_ClipDistance[0] = dot(vec4(world, 1.0), clipPlane);
	gl_Position = projection * view * vec4(world, 1.0);
}
