#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct Transform {
  void translate(const glm::vec3& v) {
    position += v;
  }

  void rotateLocal(const glm::vec3& v, float angle) {
    rotation = glm::rotate(rotation, angle, v);
  }

  void rotateGlobal(const glm::vec3& v, float angle) {
    rotation = glm::rotate(rotation, angle, glm::rotate(glm::inverse(rotation), v));
  }

  glm::vec3 forward() const {
    glm::vec3 ret{ 0.0, 0.0, -1.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::vec3 up() const {
    glm::vec3 ret{ 0.0, 1.0, 0.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::vec3 right() const {
    glm::vec3 ret{ 1.0, 0.0, 0.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::mat4x4 asMatrix4() const {
    glm::mat4x4 ret = glm::identity<glm::mat4>();
    if (parentTransform)
      ret = parentTransform->asMatrix4();
    ret = glm::translate(ret, position) * glm::toMat4(rotation);
    return ret;
  }

  glm::vec3 position = glm::zero<glm::vec3>();
  glm::quat rotation = glm::identity<glm::quat>();
  Transform* parentTransform = nullptr;
};
