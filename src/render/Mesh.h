#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Mesh {
public:
    struct Vertex {
        // use plain floats to guarantee standard layout for offsetof and GPU upload
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    Mesh();
    ~Mesh();

    // set CPU-side data and upload to GPU
    void setData(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices);

    // draw the mesh (must have appropriate shader bound)
    void draw() const;

    // convenience: clear CPU-side data and delete GL buffers
    void clear();

    bool isValid() const { return indexCount_ > 0 && vao_ != 0; }

private:
    GLuint vao_{0}, vbo_{0}, ebo_{0};
    GLsizei indexCount_{0};
};
