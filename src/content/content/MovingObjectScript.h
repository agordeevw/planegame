#pragma once
#include <engine/Component/Script.h>

#include <glm/vec3.hpp>

class MovingObjectScript final : public Script {
public:
  using Script::Script;

  void initialize() override;
  void update() override;

  Object* m_chasedObject = nullptr;
  float m_forwardRotationSpeed = 1.0f;
  float m_rightRotationSpeed = 1.0f;
  glm::vec3 m_velocity{};
};
