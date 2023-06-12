#pragma once
#include <planegame/Component/Transform.h>

#include <cstdint>
#include <memory> // unique_ptr
#include <vector>

class Component;
class Scene;

class Object {
public:
  Object(Scene& scene);

  void destroy();
  void detachFromParent();
  bool addChild(Object* object);

  Object* parent() const {
    return m_parent;
  }

  const std::vector<Object*>& children() const {
    return m_children;
  }

  template <class T>
  T* getComponent() const {
    static_assert(std::is_final_v<T>, "T must be final");
    T* ret = nullptr;
    for (Component* component : m_components) {
      if (ret = dynamic_cast<T*>(component))
        break;
    }
    return ret;
  }

  template <class T>
  T* addComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must be derived from Component");
    static_assert(std::is_final_v<T>, "T must be final");
    std::unique_ptr<T> component = std::make_unique<T>(*this);
    T* ret = component.get();
    m_components.push_back(ret);
    m_scene.registerComponent(std::move(component));
    return ret;
  }

  Scene& scene() {
    return m_scene;
  }

  Transform transform;
  uint32_t tag = -1;

private:
  friend class Scene;

  Scene& m_scene;
  Object* m_parent = nullptr;
  bool m_taggedDestroyed = false;
  std::vector<Component*> m_components;
  std::vector<Object*> m_children;
};
