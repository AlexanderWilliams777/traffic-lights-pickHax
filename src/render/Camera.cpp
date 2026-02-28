#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch, float fov)
    : position_(position), worldUp_(up), yaw_(yaw), pitch_(pitch), fov_(fov)
{
    updateVectors();
}

Camera::~Camera() = default;

void Camera::updateVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front);
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    up_ = glm::normalize(glm::cross(right_, front_));
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position_, position_ + front_, up_);
}
