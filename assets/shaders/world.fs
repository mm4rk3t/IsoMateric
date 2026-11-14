#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vLightSpacePos;

out vec4 FragColor;

#include "lighting.glsl"

// See-through window: pixels of blocks that sit between the camera and the player
// are discarded inside a circle around the player, with a dithered soft edge.
uniform bool cutaway;
uniform vec3 cutawayCenter;  // player centre
uniform float cutawayRadius; // world units, measured across the screen
uniform float cutawayFloor;  // never cut below this height (what the player stands on)

const float bayer[16] = float[16](
	 0.0,  8.0,  2.0, 10.0,
	12.0,  4.0, 14.0,  6.0,
	 3.0, 11.0,  1.0,  9.0,
	15.0,  7.0, 13.0,  5.0);

void main()
{
	if (cutaway && vWorldPos.y > cutawayFloor)
	{
		vec3 v = vWorldPos - cutawayCenter;
		vec3 toCam = normalize(viewDir);
		float along = dot(v, toCam);
		if (along > 0.3) // in front of the player, towards the camera
		{
			float dist = length(v - along * toCam);
			float keep = smoothstep(cutawayRadius * 0.6, cutawayRadius, dist);
			ivec2 p = ivec2(gl_FragCoord.xy) & 3;
			if (keep <= bayer[p.y * 4 + p.x] / 16.0)
				discard;
		}
	}
	vec3 lit = shade(vColor.rgb, vWorldPos, vNormal, vLightSpacePos);
	FragColor = vec4(applyFog(lit, vWorldPos), vColor.a);
}
