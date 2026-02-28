#pragma once

#include <string>
#include <memory>
#include <glad/glad.h>

class Texture {
public:
    Texture();
    ~Texture();

    bool createFromMemory(unsigned char* data, int w, int h, int channels);
    static std::unique_ptr<Texture> loadFromFile(const std::string& path);

    void bind(int unit = 0) const;
    GLuint id() const { return id_; }

private:
    GLuint id_ = 0;
    int width_ = 0, height_ = 0, channels_ = 0;
};
