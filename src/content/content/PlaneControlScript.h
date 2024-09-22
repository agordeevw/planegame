#pragma once
#include <engine/Component/Script.h>

#include <glm/vec3.hpp>

class PlaneControlScript final : public Script {
public:
  PlaneControlScript(Object& object);

  void initialize() override;
  void update() override;

  float minThrust = 100.0f;
  float maxThrust = 1000.0f;
  float baseThrust = 200.0f;
  float currentThrust = 200.0f;
  float velocityShiftRate = 4.0f;
  float minSpeed = 5.0f;
  float maxSpeed = 25.0f;
  float maxPitchSpeed = 1.0f;
  float maxRollSpeed = 3.0f;
  float maxYawSpeed = 0.25f;
  float pitchAcceleration = 2.0f;
  float rollAcceleration = 5.0f;
  float yawAcceleration = 0.25f;
  float currentPitchSpeed = 0.0f;
  float currentRollSpeed = 0.0f;
  float currentYawSpeed = 0.0f;
  float targetSpeed = 20.0f;

private:
  glm::vec3 velocity{};
};
