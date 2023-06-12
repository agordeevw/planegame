#pragma once
#include <planegame/Component/Component.h>

#include <glm/fwd.hpp>
#include <glm/gtc/constants.hpp>

class Camera final : public Component {
public:
  Camera(Object& object) : Component(object) {}

  glm::mat4x4 viewMatrix4() const;
  glm::mat4x4 projectionMatrix4() const;

  float fov = glm::pi<float>() / 4.0f;
  float aspectRatio = 1.0f;
  bool isMain = true;
};
