#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>

#include <string>

// Owns a GL 2D texture. Move-only so two objects never delete the same GL id.
class Texture
{
public:
	Texture() = default;
	~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	bool loadFromFile(const std::string& path, bool pixelArt = false);
	// encoded image (png/jpg) in memory; flipY = false keeps glTF's top-left UV origin
	bool loadFromMemory(const unsigned char* bytes, int size, bool flipY = false, bool pixelArt = false);
	void bind(unsigned int unit = 0) const;
	GLuint id() const { return handle; }

	int width = 0, height = 0;

private:
	bool upload(unsigned char* pixels, int w, int h, int channels, bool pixelArt);
	GLuint handle = 0;
};

#endif
