#include "texture.h"
#include "log.h"

#include "stb_image.h"

#include <utility>

Texture::~Texture()
{
	if (handle)
		glDeleteTextures(1, &handle);
}

Texture::Texture(Texture&& other) noexcept
	: width(other.width), height(other.height), handle(std::exchange(other.handle, 0)) { }

Texture& Texture::operator=(Texture&& other) noexcept
{
	if (this != &other)
	{
		if (handle)
			glDeleteTextures(1, &handle);
		handle = std::exchange(other.handle, 0);
		width = other.width;
		height = other.height;
	}
	return *this;
}

bool Texture::loadFromFile(const std::string& path, bool pixelArt)
{
	stbi_set_flip_vertically_on_load(true);
	int w = 0, h = 0, channels = 0;
	unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
	if (!data)
	{
		LOG_ERROR("texture: cannot load '%s': %s", path.c_str(), stbi_failure_reason());
		return false;
	}
	return upload(data, w, h, channels, pixelArt);
}

bool Texture::loadFromMemory(const unsigned char* bytes, int size, bool flipY, bool pixelArt)
{
	stbi_set_flip_vertically_on_load(flipY);
	int w = 0, h = 0, channels = 0;
	unsigned char* data = stbi_load_from_memory(bytes, size, &w, &h, &channels, 0);
	if (!data)
	{
		LOG_ERROR("texture: cannot decode embedded image: %s", stbi_failure_reason());
		return false;
	}
	return upload(data, w, h, channels, pixelArt);
}

bool Texture::upload(unsigned char* data, int w, int h, int channels, bool pixelArt)
{
	GLenum format = channels == 1 ? GL_RED : channels == 2 ? GL_RG : channels == 3 ? GL_RGB : GL_RGBA;

	if (!handle)
		glGenTextures(1, &handle);
	glBindTexture(GL_TEXTURE_2D, handle);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), w, h, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, pixelArt ? GL_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixelArt ? GL_NEAREST : GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(data);
	width = w;
	height = h;
	return true;
}

void Texture::bind(unsigned int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, handle);
}
