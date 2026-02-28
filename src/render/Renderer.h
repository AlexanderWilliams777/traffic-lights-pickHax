#pragma once

#include "Shader.h"
#include <glm/glm.hpp>
#include "Quad.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    // initialize shader from files and set initial projection
    bool init(const std::string& vertPath, const std::string& fragPath, const glm::mat4& projection);

    void setProjection(const glm::mat4& projection);

    // draw helpers
    void drawQuad(Quad& quad, const glm::vec3& color);

private:
    Shader shader_;
};
