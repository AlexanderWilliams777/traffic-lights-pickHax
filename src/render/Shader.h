#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader();
    ~Shader();

    // Load, compile and link shaders from files. Returns true on success.
    bool load(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;
    GLuint id() const { return program_; }

    // Uniform helpers
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setMat4(const std::string& name, const glm::mat4& m) const;

private:
    GLuint program_ = 0;

    bool compileShader(GLenum type, const char* src, GLuint& outShader) const;
    std::string readFile(const std::string& path) const;
};
