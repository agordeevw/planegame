#include <planegame/Renderer/Shader.h>

void Shader::initialize(const Options& options) {
  ShaderProgram::Options vertOptions;
  vertOptions.source = options.vertexSource;
  vertOptions.type = GL_VERTEX_SHADER;
  if (!vertShader.initialize(vertOptions))
    throw std::runtime_error("shader failure");

  ShaderProgram::Options fragOptions;
  fragOptions.source = options.fragmentSource;
  fragOptions.type = GL_FRAGMENT_SHADER;
  if (!fragShader.initialize(fragOptions))
    throw std::runtime_error("shader failure");

  glCreateProgramPipelines(1, &programPipeline);
  glUseProgramStages(programPipeline, GL_VERTEX_SHADER_BIT, vertShader.program);
  glUseProgramStages(programPipeline, GL_FRAGMENT_SHADER_BIT, fragShader.program);
}
