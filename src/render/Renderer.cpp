#include "Renderer.h"
#include <iostream>

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

bool Renderer::init(const std::string& vertPath, const std::string& fragPath, const glm::mat4& projection) {
    if (!shader_.load(vertPath, fragPath)) {
        std::cerr << "Renderer: failed to load shader\n";
        return false;
    }
    shader_.use();
    shader_.setMat4("uProjection", projection);
    return true;
}

void Renderer::setProjection(const glm::mat4& projection) {
    shader_.use();
    shader_.setMat4("uProjection", projection);
}

void Renderer::drawQuad(Quad& quad, const glm::vec3& color) {
    // shader is used by Quad::draw as well, but ensure it's active
    shader_.use();
    quad.draw(shader_, color);
}
