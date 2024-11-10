#include <engine/Component/Script.h>
#include <engine/Object.h>
#include <engine/Scene.h>

#include <algorithm>

Object::Object(Scene& scene) : m_scene(scene) {}

void Object::destroy() {
  m_taggedDestroyed = true;
}

void Object::detachFromParent() {
  if (m_parent) {
    auto it = std::remove_if(m_parent->m_children.begin(), m_parent->m_children.end(), [this](Object* o) { return this == o; });
    m_children.erase(it, m_parent->m_children.end());
    m_parent = nullptr;
    transform.parentTransform = nullptr;
  }
}

bool Object::addChild(Object* object) {
  if (object->m_parent == nullptr && std::find(m_children.begin(), m_children.end(), object) == std::end(m_children)) {
    object->m_parent = this;
    object->transform.parentTransform = &transform;
    m_children.push_back(object);
    return true;
  }
  return false;
}

void Object::attachScript(Script* script) {
  std::unique_ptr<Script> component;
  component.reset(script);
  m_components.push_back(script);
  m_scene.registerComponent(std::move(component));
}
