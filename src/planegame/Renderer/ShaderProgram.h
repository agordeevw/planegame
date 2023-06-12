#pragma once
#include <planegame/StringID.h>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

struct ShaderProgram {
  struct Options {
    GLenum type;
    const char* source;
  };

  struct UniformBlock {
    struct Entry {
      std::string name;
      StringID sid;
      GLint type;
      GLint size;
      GLint offset;
      GLint stride;
    };

    const Entry* getEntry(const char* name) const;

    std::string name;
    StringID sid;
    GLuint binding;
    GLuint size;
    GLuint index;
    std::vector<Entry> entries;
  };

  struct Uniform {
    std::string name;
    StringID sid;
    GLint type;
    GLint size;
    GLint location;
  };

  bool initialize(const Options& options);
  const UniformBlock* getUniformBlock(StringID sid) const;
  const Uniform* getUniform(StringID sid) const;

  template <class T>
  void setUniform(StringID sid, const T& value) {
    const Uniform* pUniform = getUniform(sid);
    if (pUniform) {
      if constexpr (std::is_same_v<T, int>)
        glProgramUniform1i(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, float>)
        glProgramUniform1f(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, bool>)
        glProgramUniform1i(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, glm::vec2>)
        glProgramUniform2f(program, pUniform->location, value[0], value[1]);
      else if constexpr (std::is_same_v<T, glm::vec3>)
        glProgramUniform3f(program, pUniform->location, value[0], value[1], value[2]);
      else if constexpr (std::is_same_v<T, glm::vec4>)
        glProgramUniform4f(program, pUniform->location, value[0], value[1], value[2], value[3]);
      else if constexpr (std::is_same_v<T, glm::mat4>)
        glProgramUniformMatrix4fv(program, pUniform->location, 1, GL_FALSE, reinterpret_cast<const float*>(&value));
      else
        static_assert(false);
    }
  }

  template <class T>
  void setUniformArray(StringID sid, uint32_t count, const T* value) {
    const Uniform* pUniform = getUniform(sid);
    if (pUniform) {
      if constexpr (std::is_same_v<T, int>)
        glProgramUniform1iv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, float>)
        glProgramUniform1fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec2>)
        glProgramUniform2fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec3>)
        glProgramUniform3fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec4>)
        glProgramUniform4fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else
        static_assert(false);
    }
  }

  GLuint program = -1;

private:
  bool checkLinkStatus(GLuint program);
  void listProgramUniforms(GLuint program);
  void debugShowUniforms();

  std::vector<UniformBlock> uniformBlocks;
  std::vector<Uniform> uniforms;
};
