#include <content/MovingObjectScript.h>
#include <engine/ScriptIncludes.h>

void MovingObjectScript::initialize() {
  m_chasedObject = &scene().getMainCamera()->object;
  m_forwardRotationSpeed = random().next(-1.0f, 1.0f);
  m_rightRotationSpeed = random().next(-1.0f, 1.0f);
  transform.position.x += random().next(-20.0f, 20.0f);
  transform.position.y += 0.1f * random().next(-10.0f, 10.0f);
  transform.position.z += random().next(-20.0f, 20.0f);
}

void MovingObjectScript::update() {
  transform.rotateLocal(transform.forward(), m_forwardRotationSpeed * time().dt);
  transform.rotateLocal(transform.right(), m_rightRotationSpeed * time().dt);
  glm::vec3 delta = m_chasedObject->transform.position - transform.position;
  glm::vec3 dir = glm::normalize(delta);
  if (input().keyPressed[SDL_SCANCODE_E]) {
    dir *= -10000.0f * (1.0f / (1.0f + glm::l2Norm(delta)));
  }
  m_velocity += dir * time().dt;
  transform.position += m_velocity * time().dt;
}
