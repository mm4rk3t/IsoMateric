#version 330 core

// A mirror quad built from gl_VertexID: two triangles, counter-clockwise seen from the
// reflecting side.
uniform mat4 view;
uniform mat4 projection;
uniform vec3 center;
uniform vec3 halfRight;
uniform vec3 halfUp;

out vec2 vUV; // 0..1 across the glass

void main()
{
	const vec2 corners[6] = vec2[6](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
	vec2 c = corners[gl_VertexID];
	vUV = c * 0.5 + 0.5;
	vec3 world = center + halfRight * c.x + halfUp * c.y;
	gl_Position = projection * view * vec4(world, 1.0);
}
