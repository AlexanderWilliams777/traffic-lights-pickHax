#include "Model.h"
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

Model::Model() = default;
Model::~Model() { meshes_.clear(); }

bool Model::loadOBJ(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    // derive base dir for materials
    std::string base_dir;
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) base_dir = path.substr(0, pos + 1);

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str(), base_dir.c_str(), true);
    if (!err.empty()) std::cerr << "tinyobj warning/error: " << err << std::endl;
    if (!ret) {
        std::cerr << "Failed to load OBJ via tinyobj: " << path << std::endl;
        return false;
    }

    std::vector<Mesh::Vertex> vertices;
    std::vector<unsigned int> indices;

    for (size_t s = 0; s < shapes.size(); ++s) {
        size_t index_offset = 0;
        const auto &mesh = shapes[s].mesh;
        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f) {
            int fv = mesh.num_face_vertices[f];
            for (int v = 0; v < fv; ++v) {
                tinyobj::index_t idx = mesh.indices[index_offset + v];
                Mesh::Vertex vert{};
                if (idx.vertex_index >= 0) {
                    vert.px = attrib.vertices[3 * idx.vertex_index + 0];
                    vert.py = attrib.vertices[3 * idx.vertex_index + 1];
                    vert.pz = attrib.vertices[3 * idx.vertex_index + 2];
                }
                if (idx.texcoord_index >= 0) {
                    vert.u = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vert.v = attrib.texcoords[2 * idx.texcoord_index + 1];
                }
                if (idx.normal_index >= 0) {
                    vert.nx = attrib.normals[3 * idx.normal_index + 0];
                    vert.ny = attrib.normals[3 * idx.normal_index + 1];
                    vert.nz = attrib.normals[3 * idx.normal_index + 2];
                }
                indices.push_back(static_cast<unsigned int>(vertices.size()));
                vertices.push_back(vert);
            }
            index_offset += fv;
        }
    }

    if (vertices.empty() || indices.empty()) {
        std::cerr << "tinyobj produced no vertices for: " << path << std::endl;
        return false;
    }

    Mesh mesh;
    mesh.setData(vertices, indices);
    meshes_.clear();
    meshes_.push_back(std::move(mesh));
    return true;
}
