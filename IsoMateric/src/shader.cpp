#include "shader.h"
#include <iostream>

Shader& Shader::use()
{
	glUseProgram(this->id);
	return *this;
}

void Shader::compile(const char* vSource, const char* fSource, const char* gSource)
{
	unsigned int vShader, fShader, gShader;

	vShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vShader, 1, &vSource, NULL);
	glCompileShader(vShader);
	checkCompileErrors(vShader, "VERTEX");

	fShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fShader, 1, &fSource, NULL);
	glCompileShader(fShader);
	checkCompileErrors(fShader, "FRAGMENT");

	
	if(gSource != nullptr)
	{
		gShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(gShader, 1, &gSource, NULL);
		glCompileShader(gShader);
		checkCompileErrors(gShader, "GEOMETRY");
	}

	this->id = glCreateProgram();
	glAttachShader(this->id, vShader);
	glAttachShader(this->id, fShader);
	if(gSource != nullptr)
		glAttachShader(this->id, gShader);
	glLinkProgram(this->id);
	checkCompileErrors(this->id, "PROGRAM");

	glDeleteShader(vShader);
	glDeleteShader(fShader);
	if(gSource != nullptr)
		glDeleteShader(gShader);

}

void Shader::checkCompileErrors(unsigned int object, std::string type)
{
	int success;
	char infoLog[1024];
	
	if (type != "PROGRAM")
	{
		glGetShaderiv(object, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			glGetShaderInfoLog(object, 1024, NULL, infoLog);
			std::cout << type << " " << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(object, GL_LINK_STATUS, &success);
		if(!success)
		{
			glGetProgramInfoLog(object, 1024, NULL, infoLog);
			std::cout << type << " " << infoLog << std::endl;
		}
	}
}


void Shader::setVec3f(const char* name, glm::vec3& value, bool useShader)
{
	if(useShader)
		this->use();
	glUniform3f(glGetUniformLocation(this->id, name), value.x, value.y, value.z);
}

void Shader::setMat4(const char* name, glm::mat4& matrix, bool useShader)
{
	if(useShader)
		this->use();
	glUniformMatrix4fv(glGetUniformLocation(this->id, name), 1, false, glm::value_ptr(matrix));
}
