#pragma once
#include <engine/Component/Component.h>

#include <glm/vec3.hpp>

enum class LightType : int {
  POINT = 0,
  DIRECTIONAL = 1,
};

class Light final : public Component {
public:
  Light(Object& object) : Component(object) {}

  LightType type{};
  glm::vec3 color{};
};
