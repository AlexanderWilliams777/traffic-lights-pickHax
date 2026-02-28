#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>

Texture::Texture() = default;

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

bool Texture::createFromMemory(unsigned char* data, int w, int h, int channels) {
    if (!data) return false;
    width_ = w; height_ = h; channels_ = channels;
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

std::unique_ptr<Texture> Texture::loadFromFile(const std::string& path) {
    int w,h,channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load image: " << path << std::endl;
        return nullptr;
    }
    std::unique_ptr<Texture> tex = std::make_unique<Texture>();
    if (!tex->createFromMemory(data, w, h, channels)) { tex.reset(); }
    stbi_image_free(data);
    return tex;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}
