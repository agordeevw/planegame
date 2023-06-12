#pragma once
#include <planegame/Component/Script.h>

#include <vector>

class MovingObjectGeneratorScript final : public Script {
public:
  using Script::Script;

  void update() override;

private:
  std::vector<Object*> generatedObjects;
};
