#pragma once
#include <engine/Component/Component.h>
#include <engine/ScriptContext.h>
#include <engine/StringID.h>

#include <glm/fwd.hpp>

#include <unordered_map>
#include <vector>

// invoke in constructor of derived scripts
#define SCRIPT_REGISTER_PROPERTY(fieldName) registerProperty({ #fieldName, fieldName })

class Script : public Component {
public:
  struct NamedProperty {
    enum class Type {
      Float,
      Vec2,
      Vec3,
    };

    NamedProperty(const char* name, float& value) {
      type = Type::Float;
      ptr = &value;
      this->name = name;
    }

    NamedProperty(const char* name, glm::vec2& value) {
      type = Type::Vec2;
      ptr = &value;
      this->name = name;
    }

    NamedProperty(const char* name, glm::vec3& value) {
      type = Type::Vec3;
      ptr = &value;
      this->name = name;
    }

    Type type;
    void* ptr = nullptr;
    bool modifiable = false;
    const char* name = nullptr;
  };

  Script(Object& object);
  virtual ~Script() = default;

  virtual void initialize() {}
  virtual void update() = 0;
  virtual const char* getName() const = 0;

protected:
  Scene& scene() { return *m_context->scene; }
  const Input& input() { return *m_context->input; }
  const Time& time() { return *m_context->time; }
  Random& random() { return *m_context->random; }
  Resources& resources() { return *m_context->resources; }
  Debug& debug() { return *m_context->debug; }

  void registerProperty(NamedProperty property);

private:
  friend class Scene;

  ScriptContext* m_context = nullptr;
  std::vector<NamedProperty> m_properties;
  std::unordered_map<StringID, std::size_t, StringIDHasher> m_mapNameIDToPropertyIndex;
};
