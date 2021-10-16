#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>
#include <glm/gtc/type_ptr.hpp>

class Shader
{
public:
	Shader(){}
	Shader& use();
	void compile(const char* vSource, const char* fSource, const char* gSource = nullptr);
	void checkCompileErrors(unsigned int object, std::string type);
	unsigned int id;

	void setVec3f(const char* name, glm::vec3& value, bool useShader = false);
	void setMat4(const char* name, glm::mat4& matrix, bool useShader = false);
};


#endif
