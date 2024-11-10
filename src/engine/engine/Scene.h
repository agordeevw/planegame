#pragma once
#include <engine/Components.h>
#include <engine/ScriptContext.h>

#include <memory>
#include <vector>

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
    else {
      components.get<T>().emplace_back(std::move(component));
    }
  }

  Camera* getMainCamera();

private:
  // setting up script context and iterate over objects
  friend class Application;

  void tagChildObjectsAndComponents(Object& object);
  void destroyObjects();
  void setScriptContext(Script* script);
  void clear();

  std::vector<std::unique_ptr<Object>> objects;
  Components components;
  struct Scripts {
    std::vector<std::unique_ptr<Script>> activeScripts;
    std::vector<std::unique_ptr<Script>> pendingScripts;
  } scripts;
  ScriptContext scriptContext;
};
