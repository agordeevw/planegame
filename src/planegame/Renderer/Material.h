#pragma once
#include <planegame/Renderer/Shader.h>

#include <cstring>
#include <memory>

class Material {
public:
  void initialize(Shader* shader);

  template <class T>
  void setIndexedValue(const char* name, int index, const T& value) {
    if (!materialUniformBlock)
      return;
    const ShaderProgram::UniformBlock::Entry* entry = materialUniformBlock->getEntry(name);
    if (!entry)
      return;
    std::memcpy(uniformStorage.get() + index * entry->stride + entry->offset, &value, sizeof(value));
  }

  template <class T>
  void setValue(const char* name, const T& value) {
    setIndexedValue(name, 0, value);
  }

  Shader* shader = nullptr;
  const ShaderProgram::UniformBlock* materialUniformBlock = nullptr;
  std::unique_ptr<char[]> uniformStorage;
};
