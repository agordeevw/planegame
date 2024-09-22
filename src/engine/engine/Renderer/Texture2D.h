#pragma once
#include <glad/glad.h>

#include <cstdint>
#include <string>

class Texture2D {
public:
  struct Options {
    std::string path;
    GLuint minFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLuint magFilter = GL_LINEAR;
  };

  void initialize(const Options& options);

  uint32_t width = -1;
  uint32_t height = -1;
  GLuint handle = -1;
};
