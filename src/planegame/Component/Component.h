#pragma once

class Object;
class Transform;

class Component {
public:
  Component(Object& object);
  virtual ~Component() = default;

  void destroy() {
    m_taggedDestroyed = true;
  }

  Object& object;
  Transform& transform;

private:
  friend class Scene;

  bool m_taggedDestroyed = false;
};
