#pragma once
#include <engine/Renderer/ShaderProgram.h>

#include <string>

class Shader {
public:
  struct Options {
    const char* source = nullptr;
  };

  void initialize(const Options& options);

  ShaderProgram vertShader;
  ShaderProgram fragShader;
  GLuint programPipeline = -1;
};
