#include "resourceManager.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "stb_image.h"

std::map<std::string, Shader> ResourceManager::shaders;
std::map<std::string, Texture> ResourceManager::textures;

Shader ResourceManager::loadShader(const char* vShader, const char* fShader, const char* gShader, std::string name)
{
	shaders[name] = loadShaderFromFile(vShader, fShader, gShader);
	return shaders[name];
}

Shader ResourceManager::getShader(std::string name)
{
	return shaders[name];
}

Texture ResourceManager::loadTexture(const char* file, bool alpha, std::string name)
{
	textures[name] = loadTextureFromFile(file, alpha);
	return textures[name];
}

/*
void ResourceManager::clear()
{
	for (auto iter : shaders)
		glDeleteProgram(iter.second.id)
	for (auto iter : textures)
		glDeleteTextures(1, &iter.second.id);
}
*/

Shader ResourceManager::loadShaderFromFile(const char* vShader, const char* fShader, const char* gShader)
{
	std::string vCode;
	std::string fCode;
	std::string gCode;

	try
	{
		std::ifstream vFile(vShader);
		std::ifstream fFile(fShader);
		std::stringstream vStream, fStream;

		vStream << vFile.rdbuf();
		fStream << fFile.rdbuf();

		vFile.close();
		fFile.close();
		
		vCode = vStream.str();
		fCode = fStream.str();


		if(fShader != nullptr)
		{
			std::ifstream gFile(gShader);
			std::stringstream gStream;
			gStream << gFile.rdbuf();
			gFile.close();
			gCode = gStream.str();

		}
	}
	catch(std::exception e)
	{
		std::cout << "SHADER::Failed to read files" << std::endl;
	}	
	
	const char* vShaderCode = vCode.c_str();
	const char* fShaderCode = fCode.c_str();
	const char* gShaderCode = gCode.c_str();
	
	Shader shader;
	shader.compile(vShaderCode, fShaderCode, gShader != nullptr ? gShaderCode : nullptr);

	return shader;
}

Texture ResourceManager::loadTextureFromFile(const char* file, bool alpha)
{
	Texture texture;
	if(alpha)
	{
		texture.internalFormat = GL_RGBA;
		texture.imageFormat = GL_RGBA;
	}

	int width, height, nrChannels;
	unsigned char* data = stbi_load(file, &width, &height, &nrChannels, 0);
	texture.generate(width, height, data);
	stbi_image_free(data);
	return texture;

}
