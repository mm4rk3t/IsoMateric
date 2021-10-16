#ifndef TEXTURE_H
#define TEXTURE_H

#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h>

class Texture
{
public:
	unsigned int id;
	unsigned int width, height;

	unsigned int internalFormat;
	unsigned int imageFormat;
		
	unsigned int wrapS;
	unsigned int wrapT;
	unsigned int filterMin;
	unsigned int filterMag;

	Texture();

	void generate(unsigned int width, unsigned int height, unsigned char* data);

	void bind() const;
};

#endif
