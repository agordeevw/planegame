#pragma once
#include <planegame/StringID.h>

#include <memory>
#include <unordered_map>
#include <vector>

class Mesh;
class Shader;
class Material;
class Texture2D;

class Resources {
public:
  template <class T>
  class NamedResource {
  public:
    template <class... Args>
    T* add(StringID sid, Args&&... args) {
      auto res = std::make_unique<T>(std::forward<Args>(args)...);
      T* ret = res.get();
      if (nameToResourceIdx.find(sid) != nameToResourceIdx.end())
        throw std::runtime_error("resource sid collision");
      nameToResourceIdx[std::move(sid)] = resources.size();
      resources.push_back(std::move(res));
      return ret;
    }

    T* get(StringID sid) const {
      return resources[nameToResourceIdx.at(sid)].get();
    }

  private:
    std::vector<std::unique_ptr<T>> resources;
    std::unordered_map<StringID, std::size_t, StringIDHasher> nameToResourceIdx;
  };

  template <class T, class... Args>
  T* create(StringID sid, Args&&... args) {
    return resource<T>().add(sid, std::forward<Args>(args)...);
  }

  template <class T>
  T* get(StringID sid) {
    return resource<T>().get(sid);
  }

private:
  template <class T>
  NamedResource<T>& resource() {
    if constexpr (std::is_same_v<T, Mesh>) {
      return meshes;
    }
    else if constexpr (std::is_same_v<T, Shader>) {
      return shaders;
    }
    else if constexpr (std::is_same_v<T, Material>) {
      return materials;
    }
    else if constexpr (std::is_same_v<T, Texture2D>) {
      return textures2D;
    }
    else {
      static_assert(false, "Resource type not handled");
    }
  }

  NamedResource<Mesh> meshes;
  NamedResource<Shader> shaders;
  NamedResource<Material> materials;
  NamedResource<Texture2D> textures2D;
};
