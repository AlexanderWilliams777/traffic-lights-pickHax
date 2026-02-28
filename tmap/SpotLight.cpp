#include "SpotLight.h"

void SpotLight::UseLight(GLuint ambientIntensityLocation, GLuint ambientColourLocation,
    GLuint diffuseIntensityLocation, GLuint positionLocation, GLuint directionLocation,
    GLuint constantLocation, GLuint linearLocation, GLuint exponentLocation,
    GLuint edgeLocation) const
{
	glUniform3f(ambientColourLocation, colour.x, colour.y, colour.z);
	glUniform1f(ambientIntensityLocation, ambientIntensity);
	glUniform3f(positionLocation, position.x, position.y, position.z);
	glUniform1f(diffuseIntensityLocation, diffuseIntensity);
	glUniform1f(constantLocation, constant);
	glUniform1f(linearLocation, linear);
	glUniform1f(exponentLocation, exponent);
	glUniform3f(directionLocation, direction.x, direction.y, direction.z);
	glUniform1f(edgeLocation, procEdge);
}

SpotLight::~SpotLight()
{
}