#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader() = default;
Shader::~Shader() {
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

std::string Shader::readFile(const std::string& path) const {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool Shader::compileShader(GLenum type, const char* src, GLuint& outShader) const {
    outShader = glCreateShader(type);
    glShaderSource(outShader, 1, &src, nullptr);
    glCompileShader(outShader);
    GLint success = 0;
    glGetShaderiv(outShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetShaderInfoLog(outShader, sizeof(info), nullptr, info);
        std::cerr << "Shader compile error: " << info << std::endl;
        glDeleteShader(outShader);
        outShader = 0;
        return false;
    }
    return true;
}

bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vsrc = readFile(vertexPath);
    std::string fsrc = readFile(fragmentPath);
    if (vsrc.empty() || fsrc.empty()) {
        std::cerr << "Failed to read shader files: " << vertexPath << " , " << fragmentPath << std::endl;
        return false;
    }

    GLuint vsh = 0, fsh = 0;
    if (!compileShader(GL_VERTEX_SHADER, vsrc.c_str(), vsh)) return false;
    if (!compileShader(GL_FRAGMENT_SHADER, fsrc.c_str(), fsh)) { glDeleteShader(vsh); return false; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsh);
    glAttachShader(prog, fsh);
    glLinkProgram(prog);
    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetProgramInfoLog(prog, sizeof(info), nullptr, info);
        std::cerr << "Program link error: " << info << std::endl;
        glDeleteShader(vsh);
        glDeleteShader(fsh);
        glDeleteProgram(prog);
        return false;
    }

    // Cleanup shaders
    glDeleteShader(vsh);
    glDeleteShader(fsh);

    if (program_ != 0) glDeleteProgram(program_);
    program_ = prog;
    return true;
}

void Shader::use() const {
    if (program_ != 0) glUseProgram(program_);
}

static GLint getUniform(GLuint prog, const std::string& name) {
    return glGetUniformLocation(prog, name.c_str());
}

void Shader::setBool(const std::string& name, bool value) const {
    if (!program_) return;
    glUniform1i(getUniform(program_, name), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
    if (!program_) return;
    glUniform1i(getUniform(program_, name), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    if (!program_) return;
    glUniform1f(getUniform(program_, name), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) const {
    if (!program_) return;
    glUniform3f(getUniform(program_, name), v.x, v.y, v.z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& m) const {
    if (!program_) return;
    glUniformMatrix4fv(getUniform(program_, name), 1, GL_FALSE, &m[0][0]);
}
