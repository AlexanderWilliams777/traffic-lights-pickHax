#include "Texture.h"
#include <iostream>
#include "stb_image.h"

int Texture::textureCount = 0;

Texture::Texture()
	: textureID(0), width(0), height(0), channels(0)
{
}

Texture::Texture(const std::string& path)
	: Texture()
{
	LoadFromFile(path);
}

bool Texture::LoadFromFile(const std::string& path)
{
	filePath = path;

	stbi_set_flip_vertically_on_load(true);

	unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	
	if (!data)
	{
		std::cerr << "[TEXTURE ERROR] Failed to load: " << path << "\n";
		std::cerr << "Reason: " << stbi_failure_reason() << "\n";
		return false;
	}

	GLenum format;
	if (channels == 1)		format = GL_RED;
	else if (channels == 3)	format = GL_RGB;
	else if (channels == 4)	format = GL_RGBA;
	else
	{
		std::cerr << "[TEXTURE ERROR] Unsupported channel count: " << channels << "\n";
		stbi_image_free(data);
		return false;
	}

	// Generate OpenGL texture
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(data);

	std::cout << "[TEXTURE] Loaded: " << path << " (" << width << "x"
		<< height << ", " << channels << " channels)\n";

	textureCount++;
}

void Texture::Bind(GLuint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::Unbind() const
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Clear()
{
	if (textureID != 0)
	{
		glDeleteTextures(1, &textureID);
		textureCount--;
	}
	textureID = 0;
}

Texture::~Texture()
{
	Clear();
}