#pragma once

#include <GL\glew.h>
#include <glm\glm.hpp>

class Light
{
public:
	Light()
		: colour(glm::vec3(1.0f, 1.0f, 1.0f)), ambientIntensity(1.0f), diffuseIntensity(0.0f)
	{
	}

	Light(glm::vec3 colour, GLfloat aIntensity)
		: colour(colour), ambientIntensity(aIntensity), diffuseIntensity(0.0f)
	{
	}

	Light(GLfloat red, GLfloat green, GLfloat blue, GLfloat aIntensity, GLfloat dIntensity)
		: colour(glm::vec3(red, green, blue)), ambientIntensity(aIntensity), diffuseIntensity(dIntensity)
	{
	}

	~Light();

protected:
	glm::vec3 colour;
	GLfloat ambientIntensity;
	GLfloat diffuseIntensity;
};

