#pragma once
#include "PointLight.h"
class SpotLight :
    public PointLight
{
public:
    SpotLight() : PointLight(), direction(glm::vec3(0.0f, -1.0f, 0.0f)), edge(0.0f), procEdge(0.0f) {}
    SpotLight(GLfloat red, GLfloat green, GLfloat blue, GLfloat aIntensity, GLfloat dIntensity, GLfloat xPos, GLfloat yPos, GLfloat zPos, GLfloat xDir, GLfloat yDir, GLfloat zDir, GLfloat con, GLfloat lin, GLfloat exp, GLfloat edg)
        : PointLight(red, green, blue, aIntensity, dIntensity, xPos, yPos, zPos, con, lin, exp), direction(glm::vec3(xDir, yDir, zDir)), edge(edg), procEdge(glm::radians(edge)) {}

    void UseLight(GLuint ambientIntensityLocation, GLuint ambientColourLocation,
        GLuint diffuseIntensityLocation, GLuint positionLocation, GLuint directionLocation,
        GLuint constantLocation, GLuint linearLocation, GLuint exponentLocation,
        GLuint edgeLocation) const;

    ~SpotLight();

private:
    glm::vec3 direction;

    GLfloat edge, procEdge;
};

