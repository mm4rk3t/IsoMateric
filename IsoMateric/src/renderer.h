#ifndef RENDERER_H
#define RENDERER_H

#include "shader.h"
#include "texture.h"

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>


class Renderer
{
public:
	Renderer(Shader& shader);
	~Renderer();
	void draw(glm::vec3 pos = glm::vec3(0.0f), glm::vec3 size = glm::vec3(1.0f), float rotation = 0.0f, glm::vec3 color = glm::vec3(1.0f));

	Shader shader;

private:

	unsigned int vao;
	void initData();
};

#endif
