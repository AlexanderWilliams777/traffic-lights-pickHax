#include "Quad.h"
#include <vector>

static void fillVerts(float x, float y, float w, float h, float* out)
{
    // Triangles: (x,y), (x+w,y), (x+w,y+h) and (x,y), (x+w,y+h), (x,y+h)
    out[0] = x; out[1] = y;
    out[2] = x + w; out[3] = y;
    out[4] = x + w; out[5] = y + h;
    out[6] = x; out[7] = y;
    out[8] = x + w; out[9] = y + h;
    out[10] = x; out[11] = y + h;
}

Quad::Quad(float x, float y, float w, float h)
    : x_(x), y_(y), w_(w), h_(h)
{
    initBuffers();
    update(x, y, w, h);
}

Quad::~Quad()
{
    if (VBO_) glDeleteBuffers(1, &VBO_);
    if (VAO_) glDeleteVertexArrays(1, &VAO_);
}

void Quad::initBuffers()
{
    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &VBO_);
    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Quad::update(float x, float y, float w, float h)
{
    x_ = x; y_ = y; w_ = w; h_ = h;
    float verts[12];
    fillVerts(x_, y_, w_, h_, verts);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Quad::draw(Shader& shader, const glm::vec3& color) const
{
    shader.use();
    shader.setVec3("uColor", color);
    glBindVertexArray(VAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

Quad::Quad(Quad&& other) noexcept
    : VAO_(other.VAO_), VBO_(other.VBO_), x_(other.x_), y_(other.y_), w_(other.w_), h_(other.h_)
{
    other.VAO_ = 0; other.VBO_ = 0;
}

Quad& Quad::operator=(Quad&& other) noexcept
{
    if (this != &other) {
        if (VBO_) glDeleteBuffers(1, &VBO_);
        if (VAO_) glDeleteVertexArrays(1, &VAO_);
        VAO_ = other.VAO_; VBO_ = other.VBO_;
        x_ = other.x_; y_ = other.y_; w_ = other.w_; h_ = other.h_;
        other.VAO_ = 0; other.VBO_ = 0;
    }
    return *this;
}
