#pragma once
#include <planegame/Component/Component.h>

#include <glm/vec3.hpp>

class Light final : public Component {
public:
  Light(Object& object) : Component(object) {}

  glm::vec3 color{};
};
