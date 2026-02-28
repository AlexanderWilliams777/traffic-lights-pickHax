#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"

class Quad {
public:
    Quad(float x, float y, float w, float h);
    ~Quad();

    void update(float x, float y, float w, float h);
    void draw(Shader& shader, const glm::vec3& color) const;

    // Non-copyable
    Quad(const Quad&) = delete;
    Quad& operator=(const Quad&) = delete;
    // Movable
    Quad(Quad&& other) noexcept;
    Quad& operator=(Quad&& other) noexcept;

private:
    GLuint VAO_{0}, VBO_{0};
    float x_, y_, w_, h_;
    void initBuffers();
};
