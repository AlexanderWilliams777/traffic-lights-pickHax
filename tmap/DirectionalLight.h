#pragma once
#include "Light.h"
class DirectionalLight :
    public Light
{
public:
    DirectionalLight()
        : Light(), direction(0.0f, -1.0f, 0.0f)
    {}

    DirectionalLight(glm::vec3 colour, GLfloat aIntensity)
        : Light(colour, aIntensity), direction(0.0f, -1.0f, 0.0f)
    {}

    DirectionalLight(GLfloat red, GLfloat green, GLfloat blue, GLfloat aIntensity, GLfloat dIntensity, GLfloat xDir, GLfloat yDir, GLfloat zDir)
        : Light(red, green, blue, aIntensity, dIntensity), direction(glm::vec3(xDir, yDir, zDir))
    {}

    void UseLight(GLuint ambientIntensityLocation, GLuint ambientColourLocation,
        GLuint diffuseIntensityLocation, GLuint directionLocation) const;

    ~DirectionalLight();

private:
    glm::vec3 direction;
};

