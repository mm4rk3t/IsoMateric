#version 330 core

// Bloom step 3: add the blurred highlights back (drawn with additive blending).
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D bloom;
uniform float intensity;
uniform vec3 viewOrigin;
uniform vec3 viewSize;

void main()
{
	vec2 uv = (gl_FragCoord.xy - viewOrigin.xy) / viewSize.xy;
	FragColor = vec4(texture(bloom, uv).rgb * intensity, 1.0);
}
