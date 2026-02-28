#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <GL\glew.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\quaternion.hpp>
#include <glm\gtx\quaternion.hpp>

#include <GLFW/glfw3.h>

class MyCamera
{
public:
	MyCamera();
	MyCamera(glm::vec3 startPosition, GLfloat startYaw, GLfloat startPitch, GLfloat startRoll, GLfloat startMovementSpeed, GLfloat startTurnSpeed);

	void keyControl(bool* keys, GLfloat deltaTime);

	void mouseControl(float xOffset, float yOffset);

	glm::mat4 calculateViewMatrix() const;

	glm::vec3 getPosition() const { return position; }

	void updateVectors();

	~MyCamera();

private:
	glm::vec3 position, front, up, right, worldUp;

	glm::quat orientation;

	GLfloat yaw, pitch, roll, movementSpeed, turnSpeed;

};

