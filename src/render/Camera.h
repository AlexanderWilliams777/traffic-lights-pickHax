#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
           const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f, float pitch = 0.0f, float fov = 45.0f);
    ~Camera();

    glm::mat4 getViewMatrix() const;

    void setPosition(const glm::vec3& pos) { position_ = pos; updateVectors(); }
    const glm::vec3& position() const { return position_; }

    float fov() const { return fov_; }
    void setFov(float f) { fov_ = f; }

private:
    void updateVectors();

    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 worldUp_;

    float yaw_;
    float pitch_;
    float fov_;
};
