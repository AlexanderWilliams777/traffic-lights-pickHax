#pragma once
#include "Light.h"
class PointLight :
    public Light
{
public:
    PointLight() : Light(), position(glm::vec3(0.0f, 0.0f, 0.0f)), constant(1.0f), linear(0.0f), exponent(0.0f) {}
    PointLight(GLfloat red, GLfloat green, GLfloat blue, GLfloat aIntensity, GLfloat dIntensity, GLfloat xPos, GLfloat yPos, GLfloat zPos, GLfloat con, GLfloat lin, GLfloat exp)
        : Light(red, green, blue, aIntensity, dIntensity), position(glm::vec3(xPos, yPos, zPos)), constant(con), linear(lin), exponent(exp) {}

    void UseLight(GLuint ambientIntensityLocation, GLuint ambientColourLocation,
        GLuint diffuseIntensityLocation, GLuint positionLocation, GLuint constantLocation, GLuint linearLocation, GLuint exponentLocation) const;

    ~PointLight();

protected:
    glm::vec3 position;

    GLfloat constant, linear, exponent;
};

