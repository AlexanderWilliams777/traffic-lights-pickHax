#include "Mesh.h"
#include <iostream>

Mesh::Mesh() = default;

Mesh::~Mesh() {
    clear();
}

void Mesh::setData(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices) {
    indexCount_ = static_cast<GLsizei>(indices.size());
    if (vao_ == 0) glGenVertexArrays(1, &vao_);
    if (vbo_ == 0) glGenBuffers(1, &vbo_);
    if (ebo_ == 0) glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // layout: location 0 = position (vec3), 1 = normal (vec3), 2 = uv (vec2)
    GLsizei stride = sizeof(Vertex);
    // positions at offset 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // normals after 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
    // uv after 6 floats
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (indexCount_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::clear() {
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    indexCount_ = 0;
}

