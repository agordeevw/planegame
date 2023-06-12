#include <planegame/Component/Camera.h>
#include <planegame/Object.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

glm::mat4x4 Camera::viewMatrix4() const {
  return glm::lookAt(object.transform.position, object.transform.position + object.transform.forward(), object.transform.up());
}

glm::mat4x4 Camera::projectionMatrix4() const {
  float f = 1.0f / glm::tan(fov * 0.5f);
  float zNear = 0.01f;
  return glm::mat4(
    f / aspectRatio, 0.0f, 0.0f, 0.0f,
    0.0f, f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, zNear, 0.0f);
}
