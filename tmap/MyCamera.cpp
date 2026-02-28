#include "MyCamera.h"

MyCamera::MyCamera() {}

MyCamera::MyCamera(glm::vec3 startPosition, GLfloat startYaw, GLfloat startPitch, GLfloat startRoll, GLfloat startMovementSpeed, GLfloat startTurnSpeed)
{
	position = startPosition;
	movementSpeed = startMovementSpeed;
	turnSpeed = startTurnSpeed;

	orientation = glm::angleAxis(glm::radians(startYaw), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::radians(startPitch), glm::vec3(1, 0, 0)) * glm::angleAxis(glm::radians(startRoll), glm::vec3(0, 0, 1));

	orientation = glm::normalize(orientation);

	updateVectors();
}

void MyCamera::keyControl(bool* keys, GLfloat deltaTime)
{

	GLfloat velocity = movementSpeed * deltaTime;

	GLfloat rotateVelocity = turnSpeed * deltaTime * 1000;
	
	if (keys[GLFW_KEY_W])
	{
		position += front * velocity;
	}

	if (keys[GLFW_KEY_S])
	{
		position -= front * velocity;
	}

	if (keys[GLFW_KEY_D])
	{
		position += right * velocity;
	}

	if (keys[GLFW_KEY_A])
	{
		position -= right * velocity;
	}

	if (keys[GLFW_KEY_LEFT_CONTROL])
	{
		position -= up * velocity;
	}

	if (keys[GLFW_KEY_SPACE])
	{
		position += up * velocity;
	}

	if (keys[GLFW_KEY_Q])
	{
		glm::quat qRoll = glm::angleAxis(glm::radians(rotateVelocity), front);

		orientation = glm::normalize(qRoll * orientation);
		updateVectors();
	}

	if (keys[GLFW_KEY_E])
	{
		glm::quat qRoll = glm::angleAxis(glm::radians(-rotateVelocity), front);

		orientation = glm::normalize(qRoll * orientation);
		updateVectors();
	}

	if (keys[GLFW_KEY_RIGHT])
	{
		glm::quat qYaw = glm::angleAxis(glm::radians(-rotateVelocity), up);

		orientation = glm::normalize(qYaw * orientation);
		updateVectors();
	}

	if (keys[GLFW_KEY_LEFT])
	{
		glm::quat qYaw = glm::angleAxis(glm::radians(rotateVelocity), up);

		orientation = glm::normalize(qYaw * orientation);
		updateVectors();
	}

	if (keys[GLFW_KEY_UP])
	{
		glm::quat qPitch = glm::angleAxis(glm::radians(rotateVelocity), right);

		orientation = glm::normalize(qPitch * orientation);
		updateVectors();
	}

	if (keys[GLFW_KEY_DOWN])
	{
		glm::quat qPitch = glm::angleAxis(glm::radians(-rotateVelocity), right);

		orientation = glm::normalize(qPitch * orientation);
		updateVectors();
	}

}

void MyCamera::mouseControl(float xOffset, float yOffset)
{
	xOffset *= -turnSpeed;
	yOffset *= turnSpeed;

	glm::quat qYaw = glm::angleAxis(
		glm::radians(xOffset),
		glm::vec3(0, 1, 0)   // world up
	);

	glm::quat qPitch = glm::angleAxis(
		glm::radians(yOffset),
		right              // camera local right
	);

	orientation = glm::normalize(qYaw * qPitch * orientation);
	updateVectors();
}

void MyCamera::updateVectors()
{
	front = glm::normalize(orientation * glm::vec3(0, 0, -1));
	right = glm::normalize(orientation * glm::vec3(1, 0, 0));
	up = glm::normalize(orientation * glm::vec3(0, 1, 0));
}

glm::mat4 MyCamera::calculateViewMatrix() const
{
	return glm::lookAt(position, position + front, up);
}

MyCamera::~MyCamera()
{

}