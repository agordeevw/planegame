#include <engine/Component/Transform.h>

void Transform::translate(const glm::vec3& v) {
  position += v;
}

void Transform::rotateLocal(const glm::vec3& v, float angle) {
  rotation = glm::rotate(rotation, angle, v);
}

void Transform::rotateGlobal(const glm::vec3& v, float angle) {
  rotation = glm::rotate(rotation, angle, glm::rotate(glm::inverse(rotation), v));
}

glm::vec3 Transform::worldPosition() const {
  glm::vec3 ret = position;
  Transform* parent = parentTransform;
  while (parent) {
    ret = glm::rotate(parent->rotation, ret) + parent->position;
    parent = parent->parentTransform;
  }
  return ret;
}

glm::vec3 Transform::forward() const {
  glm::vec3 ret{ 0.0, 0.0, -1.0 };
  ret = rotation * ret;
  return ret;
}

glm::vec3 Transform::up() const {
  glm::vec3 ret{ 0.0, 1.0, 0.0 };
  ret = rotation * ret;
  return ret;
}

glm::vec3 Transform::right() const {
  glm::vec3 ret{ 1.0, 0.0, 0.0 };
  ret = rotation * ret;
  return ret;
}

glm::mat4x4 Transform::asMatrix4() const {
  glm::mat4x4 ret = glm::identity<glm::mat4>();
  if (parentTransform)
    ret = parentTransform->asMatrix4();
  ret = glm::translate(ret, position) * glm::toMat4(rotation);
  return ret;
}
