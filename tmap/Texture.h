#pragma once

#include <string>

#include <GL\glew.h>

class Texture
{
public:
	Texture();
	explicit Texture(const std::string& path);
	
	bool LoadFromFile(const std::string& path);
	void Bind(GLuint unit = 0) const;
	void Unbind() const;
	void Clear();


	int GetWidth() const { return width; }
	int GetHeight() const { return height; }
	GLuint GetID() const { return textureID; }

	static const int getTextureCount() { return textureCount; }
	
	~Texture();

private:
	GLuint textureID;
	int width, height, channels;

	std::string filePath;
	static int textureCount;
};

