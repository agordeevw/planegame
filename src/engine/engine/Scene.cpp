#include <engine/Component/Camera.h>
#include <engine/Component/Light.h>
#include <engine/Component/MeshRenderer.h>
#include <engine/Component/Script.h>
#include <engine/Object.h>
#include <engine/Scene.h>

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
  for (auto& camera : components.get<Camera>()) {
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

  deleteTagged(components.get<Camera>());
  deleteTagged(components.get<Light>());
  deleteTagged(components.get<MeshRenderer>());
  deleteTagged(scripts.activeScripts);

  deleteTagged(objects);
}

void Scene::setScriptContext(Script* script) {
  script->m_context = &scriptContext;
}

void Scene::clear() {
  scripts.activeScripts.clear();
  scripts.pendingScripts.clear();
  components.cameras.clear();
  components.lights.clear();
  components.meshRenderers.clear();
  objects.clear();
}
