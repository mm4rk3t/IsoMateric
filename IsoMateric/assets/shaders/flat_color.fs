#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 color;
uniform vec3 viewPos;
uniform int nrLights;


struct Light
{
	vec3 position;
	vec3 color;
	float intensity;
};

#define MAX_NR_LIGHTS 256
uniform Light lights[MAX_NR_LIGHTS];

void main()
{
	// some variables
	vec3 norm = normalize(Normal);

	// ambient
	vec3 ambient = color * 0.3;

	// diffuse
	vec3 diffuse;

	for(int i = 0; i < nrLights; i++)	
	{
		
		vec3 lightDir = normalize(lights[i].position - FragPos);
		float diff =  max(dot(norm, lightDir), 0.0f);
		diffuse += diff * lights[i].color * lights[i].intensity;
	}

	// result
	vec3 result = ambient + diffuse;
	FragColor = vec4(result, 1.0f);
}
