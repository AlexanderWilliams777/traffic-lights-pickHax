#pragma once

#include "Mesh.h"
#include <string>
#include <vector>

class Model {
public:
    Model();
    ~Model();

    // Load a simple OBJ file (positions/uvs/normals supported). Creates a single mesh.
    bool loadOBJ(const std::string& path);

    // Access mesh
    const std::vector<Mesh>& meshes() const { return meshes_; }
    Mesh* mesh(size_t i) { return (i < meshes_.size()) ? &meshes_[i] : nullptr; }

private:
    std::vector<Mesh> meshes_;
};
