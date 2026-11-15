#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 iOffset;
layout (location = 3) in vec3 iScale;
layout (location = 4) in vec4 iColor;
layout (location = 5) in float iEmissive;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
uniform vec4 clipPlane; // mirror passes cut away what is behind the glass (GL_CLIP_DISTANCE0)
uniform float shadowNormalOffset; // world units; pushes the shadow lookup off the surface

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
out vec4 vLightSpacePos;
out float vEmissive;

void main()
{
	// boxes are only translated and scaled along the axes, so the normal is unchanged
	vec3 world = iOffset + aPos * iScale;
	vWorldPos = world;
	vNormal = aNormal;
	vColor = iColor;
	vEmissive = iEmissive;
	vLightSpacePos = lightSpace * vec4(world + aNormal * shadowNormalOffset, 1.0);
	gl_ClipDistance[0] = dot(vec4(world, 1.0), clipPlane);
	gl_Position = projection * view * vec4(world, 1.0);
}
