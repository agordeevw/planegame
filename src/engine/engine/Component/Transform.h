#pragma once
#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>

class Transform {
public:
  void translate(const glm::vec3& v);
  void rotateLocal(const glm::vec3& v, float angle);
  void rotateGlobal(const glm::vec3& v, float angle);

  glm::vec3 worldPosition() const;

  glm::vec3 forward() const;
  glm::vec3 up() const;
  glm::vec3 right() const;

  glm::mat4x4 asMatrix4() const;

  glm::vec3 position = glm::vec3{ 0.0f, 0.0f, 0.0f };
  glm::quat rotation = glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f };
  Transform* parentTransform = nullptr;
};
