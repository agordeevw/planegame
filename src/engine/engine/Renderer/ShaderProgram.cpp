#include <engine/Renderer/ShaderProgram.h>

#include <algorithm>

const ShaderProgram::UniformBlock::Entry* ShaderProgram::UniformBlock::getEntry(const char* name) const {
  auto it = std::find_if(entries.begin(), entries.end(), [name](const Entry& entry) { return entry.name == name; });
  if (it != entries.end())
    return &(*it);
  else
    return nullptr;
}

bool ShaderProgram::initialize(const Options& options) {
  const char* vertexShaderPrefix = "#version 460\n"
                                   "#define VERTEX_SHADER\n"
                                   "out gl_PerVertex {\n"
                                   "  vec4 gl_Position;\n"
                                   "  float gl_PointSize;\n"
                                   "  float gl_ClipDistance[];\n"
                                   "};\n";
  const char* fragmentShaderPrefix = "#version 460\n"
                                     "#define FRAGMENT_SHADER\n";
  if (options.type == GL_VERTEX_SHADER) {
    const char* sources[2] = { vertexShaderPrefix, options.source };
    program = glCreateShaderProgramv(options.type, 2, sources);
  }
  else if (options.type == GL_FRAGMENT_SHADER) {
    const char* sources[2] = { fragmentShaderPrefix, options.source };
    program = glCreateShaderProgramv(options.type, 2, sources);
  }

  bool status = checkLinkStatus(program);

  if (status) {
    listProgramUniforms(program);
  }
  return status;
}

const ShaderProgram::UniformBlock* ShaderProgram::getUniformBlock(StringID sid) const {
  auto it = std::find_if(uniformBlocks.begin(), uniformBlocks.end(), [sid](const UniformBlock& block) { return block.sid == sid; });
  if (it != uniformBlocks.end())
    return &(*it);
  else
    return nullptr;
}

const ShaderProgram::Uniform* ShaderProgram::getUniform(StringID sid) const {
  auto it = std::find_if(uniforms.begin(), uniforms.end(), [sid](const Uniform& block) { return block.sid == sid; });
  if (it != uniforms.end())
    return &(*it);
  else
    return nullptr;
}

bool ShaderProgram::checkLinkStatus(GLuint program) {
  GLint linkStatus;
  glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus == GL_FALSE) {
    char buffer[1024];
    glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
    printf("shader link failure: %s\n", buffer);
  }
  return linkStatus == GL_TRUE;
}

void ShaderProgram::listProgramUniforms(GLuint program) {
  GLint numUniformBlocks;
  glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numUniformBlocks);
  uniformBlocks.resize(numUniformBlocks);
  for (GLint i = 0; i < numUniformBlocks; i++) {
    char uniformBlockNameBuffer[128];
    glGetProgramResourceName(program, GL_UNIFORM_BLOCK, i, sizeof(uniformBlockNameBuffer), nullptr, uniformBlockNameBuffer);
    GLenum props[]{ GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE };
    const GLuint numProps = sizeof(props) / sizeof(props[0]);
    GLint values[numProps];
    glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, i, numProps, props, numProps, nullptr, values);
    uniformBlocks[i].name = uniformBlockNameBuffer;
    uniformBlocks[i].sid = makeSID(uniformBlockNameBuffer);
    uniformBlocks[i].binding = values[0];
    uniformBlocks[i].size = values[1];
    uniformBlocks[i].index = i;
  }

  GLint numUniforms;
  glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms);
  for (GLint i = 0; i < numUniforms; i++) {
    char uniformNameBuffer[128];
    glGetProgramResourceName(program, GL_UNIFORM, i, sizeof(uniformNameBuffer), nullptr, uniformNameBuffer);
    GLenum props[]{ GL_TYPE, GL_ARRAY_SIZE, GL_OFFSET, GL_BLOCK_INDEX, GL_ARRAY_STRIDE, GL_LOCATION };
    const GLuint numProps = sizeof(props) / sizeof(props[0]);
    GLint values[numProps];
    glGetProgramResourceiv(program, GL_UNIFORM, i, numProps, props, numProps, nullptr, values);
    GLint blockIndex = values[3];
    if (blockIndex != -1) {
      UniformBlock::Entry entry;
      entry.name = uniformNameBuffer;
      entry.sid = makeSID(uniformNameBuffer);
      entry.type = values[0];
      entry.size = values[1];
      entry.offset = values[2];
      entry.stride = values[4];
      uniformBlocks[blockIndex].entries.push_back(std::move(entry));
    }
    else {
      Uniform uniform;
      uniform.name = uniformNameBuffer;
      uniform.sid = makeSID(uniformNameBuffer);
      uniform.type = values[0];
      uniform.size = values[1];
      uniform.location = values[5];
      uniforms.push_back(std::move(uniform));
    }
  }
}

void ShaderProgram::debugShowUniforms() {
  printf("ShaderProgram\n");
  for (GLint i = 0; i < uniformBlocks.size(); i++) {
    printf("  UniformBlock %s: binding %d, size %d\n", uniformBlocks[i].name.c_str(), uniformBlocks[i].binding, uniformBlocks[i].size);
    for (UniformBlock::Entry& entry : uniformBlocks[i].entries) {
      printf("    Entry %s: type %d, array_size %d, offset %d, array_stride %d\n",
        entry.name.c_str(), entry.type, entry.size, entry.offset, entry.stride);
    }
  }

  for (GLint i = 0; i < uniforms.size(); i++) {
    printf("  Uniform %s: type %d, array_size %d, location %d\n",
      uniforms[i].name.c_str(), uniforms[i].type, uniforms[i].size, uniforms[i].location);
  }
}
