#pragma once
#include <memory>
#include <vector>

class Camera;
class Light;
class MeshRenderer;

struct Components {
  Components();
  ~Components();

  template <class T>
  std::vector<std::unique_ptr<T>>& get() {
    if constexpr (std::is_same_v<T, Camera>)
      return cameras;
    else if constexpr (std::is_same_v<T, Light>)
      return lights;
    else if constexpr (std::is_same_v<T, MeshRenderer>)
      return meshRenderers;
    else
      static_assert(false, "component type not handled");
  }

  std::vector<std::unique_ptr<Camera>> cameras;
  std::vector<std::unique_ptr<Light>> lights;
  std::vector<std::unique_ptr<MeshRenderer>> meshRenderers;
};
