#pragma once
#include <planegame/Component/Component.h>

#include <vector>

class Mesh;
class Material;

class MeshRenderer final : public Component {
public:
  MeshRenderer(Object& object) : Component(object) {}

  Mesh* mesh;
  std::vector<Material*> materials;
};
