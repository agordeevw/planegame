#pragma once
#include <planegame/ScriptContext.h>

#include <memory>
#include <vector>

class Camera;
class Light;
class MeshRenderer;
class Object;
class Script;
class Component;

class Scene {
public:
  Scene();
  ~Scene();

  Object* makeObject();
  Object* findObjectWithTag(uint32_t tag);

  template <class T>
  void registerComponent(std::unique_ptr<T> component) {
    static_assert(std::is_base_of_v<Component, T>);
    if constexpr (std::is_base_of_v<Script, T>) {
      Script* baseScript = component.get();
      setScriptContext(baseScript);
      scripts.pendingScripts.emplace_back(std::move(component));
    }
    else if constexpr (std::is_same_v<T, Camera>) {
      components.cameras.emplace_back(std::move(component));
    }
    else if constexpr (std::is_same_v<T, MeshRenderer>) {
      components.meshRenderers.emplace_back(std::move(component));
    }
    else if constexpr (std::is_same_v<T, Light>) {
      components.lights.emplace_back(std::move(component));
    }
    else {
      static_assert(false, "component type not handled");
    }
  }

  Camera* getMainCamera();

  void tagChildObjectsAndComponents(Object& object);
  void destroyObjects();

private:
  // setting up script context and iterate over objects
  friend class Application;

  void setScriptContext(Script* script);

  std::vector<std::unique_ptr<Object>> objects;
  struct Components {
    std::vector<std::unique_ptr<Camera>> cameras;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<std::unique_ptr<MeshRenderer>> meshRenderers;
  } components;
  struct Scripts {
    std::vector<std::unique_ptr<Script>> activeScripts;
    std::vector<std::unique_ptr<Script>> pendingScripts;
  } scripts;
  ScriptContext scriptContext;
};
