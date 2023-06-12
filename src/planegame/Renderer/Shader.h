#pragma once
#include <planegame/Renderer/ShaderProgram.h>

class Shader {
public:
  struct Options {
    const char* vertexSource = nullptr;
    const char* fragmentSource = nullptr;
  };

  void initialize(const Options& options);

  ShaderProgram vertShader;
  ShaderProgram fragShader;
  GLuint programPipeline = -1;
};
