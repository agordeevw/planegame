#include <planegame/Renderer/Material.h>

void Material::initialize(Shader* shader) {
  this->shader = shader;
  materialUniformBlock = shader->fragShader.getUniformBlock(SID("Material"));
  if (materialUniformBlock)
    uniformStorage = std::make_unique<char[]>(materialUniformBlock->size);
}
