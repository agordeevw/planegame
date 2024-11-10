#pragma once
#include <engine/Component/Script.h>

#include <glm/vec3.hpp>

class MissileScript final : public Script {
public:
  MissileScript(Object& object);

  static const char* name() { return "MissileScript"; }

  void initialize() override;
  void update() override;
  const char* getName() const override { return name(); }

  float targetSpeed = 50.0f;
  float thrust = 100.0f;
  glm::vec3 initialVelocity{};
  float initialTime = 10.0f;

private:
  glm::vec3 velocity{};
  float timeToDie = 0.0;
};
