#include <content/PlaneChaseCameraScript.h>
#include <engine/ScriptIncludes.h>

void PlaneChaseCameraScript::initialize() {
  m_chasedObject = scene().findObjectWithTag(0);
}

void PlaneChaseCameraScript::update() {
  Transform& transform = object.transform;
  glm::vec3 forward = m_chasedObject->transform.forward();
  glm::vec3 up = m_chasedObject->transform.up();
  glm::vec3 right = m_chasedObject->transform.right();

  if (input().keyDown[SDL_SCANCODE_C]) {
    transform.rotation = glm::quatLookAt(glm::normalize(m_chasedObject->transform.position - transform.position), transform.up());
    return;
  }

  if (input().keyDown[SDL_SCANCODE_LALT]) {
    horizAngleOffset += input().mousedx * time().dt;
  }
  else {
    horizAngleOffset = 0.0f;
  }

  glm::vec3 offset = forwardOffset * forward + upOffset * up;
  offset = glm::rotate(glm::angleAxis(horizAngleOffset, up), offset);
  transform.position = m_chasedObject->transform.position + offset;
  transform.rotation = glm::quatLookAt(glm::normalize(m_chasedObject->transform.position + lookAtUpOffset * up - transform.position), m_chasedObject->transform.up());
}
