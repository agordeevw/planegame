#include <planegame/Component/Camera.h>
#include <planegame/Component/Light.h>
#include <planegame/Component/MeshRenderer.h>
#include <planegame/Component/Script.h>
#include <planegame/Object.h>
#include <planegame/Scene.h>

#include <algorithm>

Scene::Scene() {}

Scene::~Scene() = default;

Object* Scene::makeObject() {
  return objects.emplace_back(std::make_unique<Object>(*this)).get();
}

Object* Scene::findObjectWithTag(uint32_t tag) {
  for (auto& object : objects) {
    if (object->tag == tag)
      return object.get();
  }
  return nullptr;
}

Camera* Scene::getMainCamera() {
  for (auto& camera : components.cameras) {
    if (camera->isMain)
      return camera.get();
  }
  return nullptr;
}

void Scene::tagChildObjectsAndComponents(Object& object) {
  for (auto& component : object.m_components) {
    component->m_taggedDestroyed = true;
  }
  for (auto& child : object.m_children) {
    tagChildObjectsAndComponents(*child);
  }
}

void Scene::destroyObjects() {
  for (auto& object : objects) {
    if (object->m_taggedDestroyed) {
      tagChildObjectsAndComponents(*object);
    }
  }

  auto deleteTagged = [](auto& v) {
    for (auto& component : v) {
      if (component->m_taggedDestroyed) {
        component.reset();
      }
    }
    v.erase(std::remove_if(v.begin(), v.end(), [](const auto& ptr) { return ptr.get() == nullptr; }), v.end());
  };

  deleteTagged(components.cameras);
  deleteTagged(components.lights);
  deleteTagged(components.meshRenderers);
  deleteTagged(scripts.activeScripts);

  deleteTagged(objects);
}

void Scene::setScriptContext(Script* script) {
  script->m_context = &scriptContext;
}
