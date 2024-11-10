#pragma once
#include <engine/Component/Script.h>

class PlaneChaseCameraScript final : public Script {
public:
  using Script::Script;

  static const char* name() { return "PlaneChaseCameraScript"; }

  void initialize() override;
  void update() override;
  const char* getName() const override { return name(); }

  float horizAngleOffset = 0.0f;
  float forwardOffset = -5.0f; //-5.0f;
  float upOffset = 1.0f; // 1.0f;
  float lookAtUpOffset = 1.0f; // 1.0f;
  Object* m_chasedObject = nullptr;
};
