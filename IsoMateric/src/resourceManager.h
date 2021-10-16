// singleton that contains several static functions regarding textures and shaders

#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "shader.h"
#include "texture.h"

#include <map>
#include <string>

#include <glad/glad.h>

class ResourceManager
{
public:
	static std::map<std::string, Shader>	shaders;
	static std::map<std::string, Texture> textures;

	static Shader loadShader(const char* vShader, const char* fShader, const char* gShader, std::string name);
	static Shader getShader(std::string name);

	static Texture loadTexture(const char* file, bool alpha, std::string name);
	static Texture getTexture(std::string name);

	//static void clear():

private:
	ResourceManager();

	static Shader loadShaderFromFile(const char* vShader, const char* fShader, const char* gShader = nullptr);	
	static Texture loadTextureFromFile(const char* file, bool alpha);

};

#endif

