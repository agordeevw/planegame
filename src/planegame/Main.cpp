#include "Camera.h"
#include "Transform.h"

#include <SDL.h>
#include <SDL_image.h>
#include <planegame/Renderer/glad/glad.h>

#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

struct StringID {
  uint64_t value;
};

struct StringIDHasher {
  uint64_t operator()(StringID sid) const { return std::hash<uint64_t>()(sid.value); }
};

bool operator==(StringID a, StringID b) { return a.value == b.value; }

static constexpr StringID makeSID(const char str[]) {
  uint64_t ret = 1242ULL;
  for (const char* s = str; *s != 0; s++) {
    ret = uint64_t(ret) ^ (uint64_t(*s) * 12421512ULL) + 12643ULL;
  }
  return StringID{ ret };
}

#define SID(name) \
  StringID { std::integral_constant<uint64_t, makeSID(name).value>::value }

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

struct Input {
  bool keyDown[SDL_NUM_SCANCODES]{};
  bool prevKeyDown[SDL_NUM_SCANCODES]{};
  bool keyPressed[SDL_NUM_SCANCODES]{};
  bool keyReleased[SDL_NUM_SCANCODES]{};
  float mousedx = 0.0;
  float mousedy = 0.0;
};

struct Time {
  double timeSinceStart;
  float dt;
};

class Random {
public:
  template <class T>
  T next(T min, T max) {
    std::uniform_real_distribution<T> distr(min, max);
    return distr(m_generator);
  }

private:
  std::mt19937_64 m_generator;
};

class Component;
class Scene;

class Object {
public:
  Object(Scene& scene) : m_scene(scene) {}

  void destroy() {
    taggedDestroyed = true;
  }

  void detachFromParent() {
    if (m_parent) {
      auto it = std::remove_if(m_parent->m_children.begin(), m_parent->m_children.end(), [this](Object* o) { return this == o; });
      m_children.erase(it, m_parent->m_children.end());
      m_parent = nullptr;
      transform.parentTransform = nullptr;
    }
  }

  bool addChild(Object* object) {
    if (object->m_parent == nullptr && std::find(m_children.begin(), m_children.end(), object) == std::end(m_children)) {
      object->m_parent = this;
      object->transform.parentTransform = &transform;
      m_children.push_back(object);
      return true;
    }
    return false;
  }

  Object* parent() const {
    return m_parent;
  }

  const std::vector<Object*>& children() const {
    return m_children;
  }

  template <class T>
  T* getComponent() const {
    static_assert(std::is_final_v<T>, "");
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
    std::unique_ptr<T> component;
    component = std::make_unique<T>(*this);
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

  bool taggedDestroyed = false;
  Scene& m_scene;
  std::vector<Component*> m_components;
  Object* m_parent = nullptr;
  std::vector<Object*> m_children;
};

class Component {
public:
  Component(Object& object)
    : object(object), transform(object.transform) {}
  virtual ~Component() = default;

  void destroy() {
    taggedDestroyed = true;
  }

  Object& object;
  Transform& transform;

private:
  friend class Scene;

  bool taggedDestroyed = false;
};

class Camera final : public Component {
public:
  Camera(Object& object) : Component(object) {}

  glm::mat4x4 viewMatrix4() const {
    return glm::lookAt(object.transform.position, object.transform.position + object.transform.forward(), object.transform.up());
  }

  glm::mat4x4 projectionMatrix4() const {
    float f = 1.0f / glm::tan(fov * 0.5f);
    float zNear = 0.01f;
    return glm::mat4(
      f / aspectRatio, 0.0f, 0.0f, 0.0f,
      0.0f, f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, -1.0f,
      0.0f, 0.0f, zNear, 0.0f);
  }

  float fov = glm::radians(90.0f);
  float aspectRatio = 1.0f;
  bool isMain = true;
};

class Scene;
class Resources;
class Debug;

struct ScriptContext {
  Scene* scene;
  const Input* input;
  const Time* time;
  Random* random;
  Resources* resources;
  Debug* debug;
};

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

  Script(Object& object)
    : Component(object) {
    SCRIPT_REGISTER_PROPERTY(transform.position);
  }
  virtual ~Script() = default;

  virtual void initialize() {}
  virtual void update() = 0;

protected:
  Scene& scene() { return *m_context.scene; }
  const Input& input() { return *m_context.input; }
  const Time& time() { return *m_context.time; }
  Random& random() { return *m_context.random; }
  Resources& resources() { return *m_context.resources; }
  Debug& debug() { return *m_context.debug; }

  void registerProperty(NamedProperty property) {
    m_properties.push_back(property);
    m_mapNameIDToPropertyIndex[makeSID(property.name)] = m_properties.size() - 1;
  }

private:
  friend class Scene;

  ScriptContext m_context;
  std::vector<NamedProperty> m_properties;
  std::unordered_map<StringID, std::size_t, StringIDHasher> m_mapNameIDToPropertyIndex;
};

struct Mesh {
  struct VertexAttribute {
    enum class Format {
      Unknown,
      f16,
      f32,
    };

    VertexAttribute(Format format, int dimension) : format(format), dimension(dimension) {}

    // enum class Type {
    //   Unknown,
    //   Position,
    //   Normal,
    //   Tangent,
    //   Color,
    //   UV,
    // };

    // Type type = Type::Unknown;
    Format format = Format::Unknown;
    int dimension = -1;
  };

  struct SubMesh {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
  };

  enum class IndexFormat {
    Unknown,
    u16,
    u32,
  };

  struct Options {
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    const void* vertexBufferData = nullptr;
    const void* indexBufferData = nullptr;
    std::vector<VertexAttribute> attributes;
    IndexFormat indexFormat = IndexFormat::Unknown;
  };

  void initialize(const Options& options) {
    vertexAttributes = options.attributes;
    vertexCount = options.vertexCount;
    indexFormat = options.indexFormat;
    indexCount = options.indexCount;

    glCreateVertexArrays(1, &vao);
    GLuint relativeOffset = 0;
    for (uint32_t i = 0; i < vertexAttributes.size(); i++) {
      GLenum type = -1;
      GLuint typeSize = 0;
      switch (vertexAttributes[i].format) {
        case VertexAttribute::Format::f16:
          type = GL_HALF_FLOAT;
          typeSize = 2;
          break;
        case VertexAttribute::Format::f32:
          type = GL_FLOAT;
          typeSize = 4;
          break;
      }

      glEnableVertexArrayAttrib(vao, i);
      glVertexArrayAttribFormat(vao, i, vertexAttributes[i].dimension, type, GL_FALSE, relativeOffset);
      glVertexArrayAttribBinding(vao, i, 0);
      relativeOffset += typeSize * vertexAttributes[i].dimension;
    }
    vertexSize = relativeOffset;

    glCreateBuffers(1, &vertexBuffer);
    glNamedBufferStorage(vertexBuffer, vertexSize * vertexCount, options.vertexBufferData, 0);
    glCreateBuffers(1, &elementBuffer);
    glNamedBufferStorage(elementBuffer, (options.indexFormat == IndexFormat::u32 ? 4 : 2) * indexCount, options.indexBufferData, 0);
    glVertexArrayVertexBuffer(vao, 0, vertexBuffer, 0, vertexSize);
    glVertexArrayElementBuffer(vao, elementBuffer);

    SubMesh subMesh;
    subMesh.indexStart = 0;
    subMesh.indexCount = options.indexCount;
    submeshes.push_back(subMesh);
  }

  std::vector<SubMesh> submeshes;
  std::vector<VertexAttribute> vertexAttributes;
  uint32_t vertexCount = 0;
  IndexFormat indexFormat = IndexFormat::Unknown;
  uint32_t indexCount = 0;
  uint32_t vertexSize = 0;

  GLuint vao = -1;
  GLuint vertexBuffer = -1;
  GLuint elementBuffer = -1;
};

struct ShaderProgram {
  struct Options {
    GLenum type;
    const char* source;
  };

  struct UniformBlock {
    struct Entry {
      std::string name;
      StringID sid;
      GLint type;
      GLint size;
      GLint offset;
      GLint stride;
    };

    const Entry* getEntry(const char* name) const {
      auto it = std::find_if(entries.begin(), entries.end(), [name](const Entry& entry) { return entry.name == name; });
      if (it != entries.end())
        return &(*it);
      else
        return nullptr;
    }

    std::string name;
    StringID sid;
    GLuint binding;
    GLuint size;
    GLuint index;
    std::vector<Entry> entries;
  };

  struct Uniform {
    std::string name;
    StringID sid;
    GLint type;
    GLint size;
    GLint location;
  };

  bool initialize(const Options& options) {
    const char* vertexShaderPrefix = "#version 460\n"
                                     "out gl_PerVertex {\n"
                                     "  vec4 gl_Position;\n"
                                     "  float gl_PointSize;\n"
                                     "  float gl_ClipDistance[];\n"
                                     "};\n";
    const char* fragmentShaderPrefix = "#version 460\n";
    if (options.type == GL_VERTEX_SHADER) {
      const char* sources[2] = { vertexShaderPrefix, options.source };
      program = glCreateShaderProgramv(options.type, 2, sources);
    }
    else if (options.type == GL_FRAGMENT_SHADER) {
      const char* sources[2] = { fragmentShaderPrefix, options.source };
      program = glCreateShaderProgramv(options.type, 2, sources);
    }

    bool status = checkLinkStatus(program);

    if (status) {
      listProgramUniforms(program);
    }
    return status;
  }

  const UniformBlock* getUniformBlock(StringID sid) const {
    auto it = std::find_if(uniformBlocks.begin(), uniformBlocks.end(), [sid](const UniformBlock& block) { return block.sid == sid; });
    if (it != uniformBlocks.end())
      return &(*it);
    else
      return nullptr;
  }

  const Uniform* getUniform(StringID sid) const {
    auto it = std::find_if(uniforms.begin(), uniforms.end(), [sid](const Uniform& block) { return block.sid == sid; });
    if (it != uniforms.end())
      return &(*it);
    else
      return nullptr;
  }

  template <class T>
  void setUniform(StringID sid, const T& value) {
    const Uniform* pUniform = getUniform(sid);
    if (pUniform) {
      if constexpr (std::is_same_v<T, int>)
        glProgramUniform1i(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, float>)
        glProgramUniform1f(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, bool>)
        glProgramUniform1i(program, pUniform->location, value);
      else if constexpr (std::is_same_v<T, glm::vec2>)
        glProgramUniform2f(program, pUniform->location, value[0], value[1]);
      else if constexpr (std::is_same_v<T, glm::vec3>)
        glProgramUniform3f(program, pUniform->location, value[0], value[1], value[2]);
      else if constexpr (std::is_same_v<T, glm::vec4>)
        glProgramUniform4f(program, pUniform->location, value[0], value[1], value[2], value[3]);
      else if constexpr (std::is_same_v<T, glm::mat4>)
        glProgramUniformMatrix4fv(program, pUniform->location, 1, GL_FALSE, reinterpret_cast<const float*>(&value));
      else
        static_assert(false);
    }
  }

  template <class T>
  void setUniformArray(StringID sid, uint32_t count, const T* value) {
    const Uniform* pUniform = getUniform(sid);
    if (pUniform) {
      if constexpr (std::is_same_v<T, int>)
        glProgramUniform1iv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, float>)
        glProgramUniform1fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec2>)
        glProgramUniform2fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec3>)
        glProgramUniform3fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else if constexpr (std::is_same_v<T, glm::vec4>)
        glProgramUniform4fv(program, pUniform->location, count, reinterpret_cast<const float*>(value));
      else
        static_assert(false);
    }
  }

  GLuint program;

private:
  bool checkLinkStatus(GLuint program) {
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
      char buffer[1024];
      glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
      printf("shader link failure: %s\n", buffer);
    }
    return linkStatus == GL_TRUE;
  }

  void listProgramUniforms(GLuint program) {
    GLint numUniformBlocks;
    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numUniformBlocks);
    uniformBlocks.resize(numUniformBlocks);
    for (GLint i = 0; i < numUniformBlocks; i++) {
      char uniformBlockNameBuffer[128];
      glGetProgramResourceName(program, GL_UNIFORM_BLOCK, i, sizeof(uniformBlockNameBuffer), nullptr, uniformBlockNameBuffer);
      GLenum props[]{ GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE };
      const GLuint numProps = sizeof(props) / sizeof(props[0]);
      GLint values[numProps];
      glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, i, numProps, props, numProps, nullptr, values);
      uniformBlocks[i].name = uniformBlockNameBuffer;
      uniformBlocks[i].sid = makeSID(uniformBlockNameBuffer);
      uniformBlocks[i].binding = values[0];
      uniformBlocks[i].size = values[1];
      uniformBlocks[i].index = i;
    }

    GLint numUniforms;
    glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms);
    for (GLint i = 0; i < numUniforms; i++) {
      char uniformNameBuffer[128];
      glGetProgramResourceName(program, GL_UNIFORM, i, sizeof(uniformNameBuffer), nullptr, uniformNameBuffer);
      GLenum props[]{ GL_TYPE, GL_ARRAY_SIZE, GL_OFFSET, GL_BLOCK_INDEX, GL_ARRAY_STRIDE, GL_LOCATION };
      const GLuint numProps = sizeof(props) / sizeof(props[0]);
      GLint values[numProps];
      glGetProgramResourceiv(program, GL_UNIFORM, i, numProps, props, numProps, nullptr, values);
      GLint blockIndex = values[3];
      if (blockIndex != -1) {
        UniformBlock::Entry entry;
        entry.name = uniformNameBuffer;
        entry.sid = makeSID(uniformNameBuffer);
        entry.type = values[0];
        entry.size = values[1];
        entry.offset = values[2];
        entry.stride = values[4];
        uniformBlocks[blockIndex].entries.push_back(std::move(entry));
      }
      else {
        Uniform uniform;
        uniform.name = uniformNameBuffer;
        uniform.sid = makeSID(uniformNameBuffer);
        uniform.type = values[0];
        uniform.size = values[1];
        uniform.location = values[5];
        uniforms.push_back(std::move(uniform));
      }
    }
  }

  void debugShowUniforms() {
    printf("ShaderProgram\n");
    for (GLint i = 0; i < uniformBlocks.size(); i++) {
      printf("  UniformBlock %s: binding %d, size %d\n", uniformBlocks[i].name.c_str(), uniformBlocks[i].binding, uniformBlocks[i].size);
      for (UniformBlock::Entry& entry : uniformBlocks[i].entries) {
        printf("    Entry %s: type %d, array_size %d, offset %d, array_stride %d\n",
          entry.name.c_str(), entry.type, entry.size, entry.offset, entry.stride);
      }
    }

    for (GLint i = 0; i < uniforms.size(); i++) {
      printf("  Uniform %s: type %d, array_size %d, location %d\n",
        uniforms[i].name.c_str(), uniforms[i].type, uniforms[i].size, uniforms[i].location);
    }
  }

  std::vector<UniformBlock> uniformBlocks;
  std::vector<Uniform> uniforms;
};

struct Shader {
  struct Options {
    const char* vertexSource = nullptr;
    const char* fragmentSource = nullptr;
  };

  void initialize(const Options& options) {
    ShaderProgram::Options vertOptions;
    vertOptions.source = options.vertexSource;
    vertOptions.type = GL_VERTEX_SHADER;
    if (!vertShader.initialize(vertOptions))
      throw std::runtime_error("shader failure");

    ShaderProgram::Options fragOptions;
    fragOptions.source = options.fragmentSource;
    fragOptions.type = GL_FRAGMENT_SHADER;
    if (!fragShader.initialize(fragOptions))
      throw std::runtime_error("shader failure");

    glCreateProgramPipelines(1, &programPipeline);
    glUseProgramStages(programPipeline, GL_VERTEX_SHADER_BIT, vertShader.program);
    glUseProgramStages(programPipeline, GL_FRAGMENT_SHADER_BIT, fragShader.program);
  }

  ShaderProgram vertShader;
  ShaderProgram fragShader;
  GLuint programPipeline = -1;
};

struct Texture2D {
  struct Options {
    const char* path = nullptr;
    GLuint minFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLuint magFilter = GL_LINEAR;
  };

  void initialize(const Options& options) {
    SDL_Surface* surf = IMG_Load(options.path);
    GLenum glformat = GL_RGB;
    switch (surf->format->BytesPerPixel) {
      case 1:
        glformat = GL_RED;
        break;
      case 2:
        glformat = GL_RG;
        break;
      case 3:
        glformat = GL_RGB;
        break;
      case 4:
        glformat = GL_RGBA;
        break;
      default:
        throw std::runtime_error("unknown image format");
    }

    std::vector<char> data(surf->h * surf->w * surf->format->BytesPerPixel);
    for (int r = 0; r < surf->h; r++) {
      memcpy(data.data() + (surf->h - r - 1) * surf->pitch, (char*)surf->pixels + r * surf->pitch, surf->pitch);
    }
    width = surf->w;
    height = surf->h;

    glCreateTextures(GL_TEXTURE_2D, 1, &handle);
    glTextureStorage2D(handle, 1, GL_RGB8, surf->w, surf->h);
    glTextureSubImage2D(handle, 0, 0, 0, surf->w, surf->h, glformat, GL_UNSIGNED_BYTE, data.data());
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, options.minFilter);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, options.magFilter);
    SDL_FreeSurface(surf);
  }

  uint32_t width = -1;
  uint32_t height = -1;
  GLuint handle = -1;
};

struct Material {
  void initialize(Shader* shader) {
    this->shader = shader;
    materialUniformBlock = shader->fragShader.getUniformBlock(SID("Material"));
    if (materialUniformBlock)
      uniformStorage = std::make_unique<char[]>(materialUniformBlock->size);
  }

  template <class T>
  void setIndexedValue(const char* name, int index, const T& value) {
    if (!materialUniformBlock)
      return;
    const ShaderProgram::UniformBlock::Entry* entry = materialUniformBlock->getEntry(name);
    if (!entry)
      return;
    memcpy(uniformStorage.get() + index * entry->stride + entry->offset, &value, sizeof(value));
  }

  template <class T>
  void setValue(const char* name, const T& value) {
    setIndexedValue(name, 0, value);
  }

  Shader* shader = nullptr;
  const ShaderProgram::UniformBlock* materialUniformBlock = nullptr;
  std::unique_ptr<char[]> uniformStorage;
};

class MeshRenderer final : public Component {
public:
  MeshRenderer(Object& object) : Component(object) {}

  Mesh* mesh;
  std::vector<Material*> materials;
};

class Light final : public Component {
public:
  Light(Object& object) : Component(object) {}

  glm::vec3 color{};
};

class Scene {
public:
  Object* makeObject() {
    return objects.emplace_back(std::make_unique<Object>(*this)).get();
  }

  Object* findObjectWithTag(uint32_t tag) {
    for (auto& object : objects) {
      if (object->tag == tag)
        return object.get();
    }
    return nullptr;
  }

  template <class T>
  void registerComponent(std::unique_ptr<T> component) {
    static_assert(std::is_base_of_v<Component, T>);
    if constexpr (std::is_base_of_v<Script, T>) {
      Script* baseScript = component.get();
      baseScript->m_context = scriptContext;
      scripts.pendingScripts.push_back(std::move(component));
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

  Camera* getMainCamera() {
    for (auto& camera : components.cameras) {
      if (camera->isMain)
        return camera.get();
    }
    return nullptr;
  }

  void tagChildObjectsAndComponents(Object& object) {
    for (auto& component : object.m_components) {
      component->taggedDestroyed = true;
    }
    for (auto& child : object.m_children) {
      tagChildObjectsAndComponents(*child);
    }
  }

  void destroyObjects() {
    for (auto& object : objects) {
      if (object->taggedDestroyed) {
        tagChildObjectsAndComponents(*object);
      }
    }

    auto deleteTagged = [](auto& v) {
      for (auto& component : v) {
        if (component->taggedDestroyed) {
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

private:
  // setting up script context and iterate over objects
  friend class Application;

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

class Resources {
public:
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

class Debug {
public:
  struct DrawLineCommand {
    glm::vec3 verts[2];
    glm::vec3 color;
  };

  struct DrawScreenLineCommand {
    glm::vec2 verts[2];
    glm::vec3 color;
  };

  struct DrawScreenTextCommand {
    glm::vec2 topleft;
    std::string str;
  };

  void drawLine(glm::vec3 start, glm::vec3 end, glm::vec3 color) {
    DrawLineCommand command;
    command.verts[0] = start;
    command.verts[1] = end;
    command.color = color;
    m_drawLineCommands.push_back(command);
  }

  void drawScreenLine(glm::vec2 start, glm::vec2 end, glm::vec3 color) {
    DrawScreenLineCommand command;
    command.verts[0] = start;
    command.verts[1] = end;
    command.color = color;
    m_drawScreenLineCommands.push_back(command);
  }

  void drawScreenText(glm::vec2 topleft, const char* str) {
    DrawScreenTextCommand command;
    command.topleft = topleft;
    command.str = str;
    m_drawScreenTextCommands.push_back(command);
  }

  void clear() {
    m_drawLineCommands.clear();
    m_drawScreenLineCommands.clear();
    m_drawScreenTextCommands.clear();
  }

private:
  friend class Application;

  std::vector<DrawLineCommand> m_drawLineCommands;
  std::vector<DrawScreenLineCommand> m_drawScreenLineCommands;
  std::vector<DrawScreenTextCommand> m_drawScreenTextCommands;
};

class FPSCameraScript final : public Script {
public:
  using Script::Script;

  void update() override {
    Transform& transform = object.transform;

    float speed = m_speed;
    if (input().keyDown[SDL_SCANCODE_LSHIFT]) {
      speed *= 2.0;
    }

    if (input().keyDown[SDL_SCANCODE_W]) {
      transform.position += transform.forward() * speed * time().dt;
    }
    if (input().keyDown[SDL_SCANCODE_S]) {
      transform.position -= transform.forward() * speed * time().dt;
    }
    if (input().keyDown[SDL_SCANCODE_D]) {
      transform.position += transform.right() * speed * time().dt;
    }
    if (input().keyDown[SDL_SCANCODE_A]) {
      transform.position -= transform.right() * speed * time().dt;
    }
    if (input().keyDown[SDL_SCANCODE_Q]) {
      transform.position += transform.up() * speed * time().dt;
    }
    if (input().keyDown[SDL_SCANCODE_Z]) {
      transform.position -= transform.up() * speed * time().dt;
    }
    transform.rotateGlobal(glm::vec3(0.0f, 1.0f, 0.0f), -time().dt * input().mousedx);
    transform.rotateLocal(glm::vec3(1.0f, 0.0f, 0.0f), -time().dt * input().mousedy);
  }

  float m_speed = 10.0;
};

class PlaneChaseCameraScript final : public Script {
public:
  using Script::Script;

  void initialize() override {
    m_chasedObject = scene().findObjectWithTag(0);
  }

  void update() override {
    Transform& transform = object.transform;
    glm::vec3 forward = m_chasedObject->transform.forward();
    glm::vec3 up = m_chasedObject->transform.up();
    glm::vec3 right = m_chasedObject->transform.right();

    if (input().keyDown[SDL_SCANCODE_C]) {
      transform.rotation = glm::quatLookAt(glm::normalize(m_chasedObject->transform.position - transform.position), transform.up());
      return;
    }

    if (input().keyDown[SDL_SCANCODE_LALT]) {
      horizAngleOffset += input().mousedx * time().dt;
    }
    else {
      horizAngleOffset = 0.0f;
    }

    glm::vec3 offset = forwardOffset * forward + upOffset * up;
    offset = glm::rotate(glm::angleAxis(horizAngleOffset, up), offset);
    transform.position = m_chasedObject->transform.position + offset;
    transform.rotation = glm::quatLookAt(glm::normalize(m_chasedObject->transform.position + lookAtUpOffset * up - transform.position), m_chasedObject->transform.up());
  }

  float horizAngleOffset = 0.0f;
  float forwardOffset = -5.0f; //-5.0f;
  float upOffset = 1.0f; // 1.0f;
  float lookAtUpOffset = 1.0f; // 1.0f;
  Object* m_chasedObject = nullptr;
};

class PlaneControlScript final : public Script {
public:
  PlaneControlScript(Object& object) : Script(object) {
    SCRIPT_REGISTER_PROPERTY(velocityShiftRate);
    SCRIPT_REGISTER_PROPERTY(minSpeed);
    SCRIPT_REGISTER_PROPERTY(maxPitchSpeed);
    SCRIPT_REGISTER_PROPERTY(maxRollSpeed);
    SCRIPT_REGISTER_PROPERTY(maxYawSpeed);
    SCRIPT_REGISTER_PROPERTY(pitchAcceleration);
    SCRIPT_REGISTER_PROPERTY(rollAcceleration);
    SCRIPT_REGISTER_PROPERTY(yawAcceleration);
    SCRIPT_REGISTER_PROPERTY(currentPitchSpeed);
    SCRIPT_REGISTER_PROPERTY(currentRollSpeed);
    SCRIPT_REGISTER_PROPERTY(currentYawSpeed);
    SCRIPT_REGISTER_PROPERTY(targetSpeed);
  }

  void initialize() override {
    velocity = targetSpeed * transform.forward();
  }

  void update() override {
    Transform& transform = object.transform;

    const glm::vec3 forward = transform.forward();
    const glm::vec3 up = transform.up();
    const glm::vec3 right = transform.right();
    const glm::vec3 globalUp = glm::vec3{ 0.0f, 1.0f, 0.0f };

    bool stalling = false;

    float inputThrustAcceleration = 0.0f;
    if (input().keyDown[SDL_SCANCODE_LSHIFT]) {
      inputThrustAcceleration = 100.0f;
    }
    else if (input().keyDown[SDL_SCANCODE_LCTRL]) {
      inputThrustAcceleration = -100.0f;
    }

    if (inputThrustAcceleration != 0.0f) {
      currentThrust += inputThrustAcceleration * time().dt;
    }
    else {
      if (currentThrust > baseThrust) {
        currentThrust = std::clamp(currentThrust - 50.0f * time().dt, baseThrust, currentThrust);
      }
      else {
        currentThrust = std::clamp(currentThrust + 50.0f * time().dt, currentThrust, baseThrust);
      }
    }
    currentThrust = std::clamp(currentThrust, minThrust, maxThrust);

    float weight = 20.0f;
    float lift = 20.0f;
    if (targetSpeed < 15.0f) {
      stalling = true;
      lift = 10.0f - (15.0f - targetSpeed);
    }

    glm::vec3 linearAcceleration = forward * currentThrust - weight * globalUp + lift * up;
    targetSpeed = 20.0f * (glm::length(linearAcceleration) / baseThrust);

    // estimate linear acceleration vector which is used to determine base rotation speed

    float forwardDot = glm::dot(forward, linearAcceleration);
    float upDot = glm::dot(up, linearAcceleration);
    float rightDot = glm::dot(right, linearAcceleration);

    // velocity vector must become aligned with linear acceleration

    glm::vec3 targetVelocity = targetSpeed * glm::normalize(linearAcceleration);
    glm::vec3 deltaVelocity = targetVelocity - velocity;
    velocity += velocityShiftRate * deltaVelocity * time().dt;
    velocity = targetSpeed * glm::normalize(velocity);
    transform.position += velocity * time().dt;

    // rotation speed based on plane position
    float basePitchSpeed = 0.0f;
    float baseRollSpeed = 0.0f;
    float baseYawSpeed = 0.0f;
    {
      basePitchSpeed = 0.0025f * upDot;
      baseYawSpeed = -0.001f * rightDot;
    }

    // rotation acceleration based on player input
    float inputPitchAcceleration = 0.0f;
    float inputRollAcceleration = 0.0f;
    float inputYawAcceleration = 0.0f;

    if (!stalling) {
      if (input().keyDown[SDL_SCANCODE_W]) {
        inputPitchAcceleration = -pitchAcceleration;
      }
      if (input().keyDown[SDL_SCANCODE_S]) {
        inputPitchAcceleration = +pitchAcceleration;
      }
      if (input().keyDown[SDL_SCANCODE_D]) {
        inputRollAcceleration = -rollAcceleration;
      }
      if (input().keyDown[SDL_SCANCODE_A]) {
        inputRollAcceleration = +rollAcceleration;
      }
      if (input().keyDown[SDL_SCANCODE_E]) {
        inputYawAcceleration = -yawAcceleration;
      }
      if (input().keyDown[SDL_SCANCODE_Q]) {
        inputYawAcceleration = +yawAcceleration;
      }
    }

    if (inputPitchAcceleration != 0.0f) {
      currentPitchSpeed += inputPitchAcceleration * time().dt;
    }
    else {
      if (currentPitchSpeed > basePitchSpeed) {
        currentPitchSpeed = std::clamp(currentPitchSpeed - 2.0f * time().dt, basePitchSpeed, currentPitchSpeed);
      }
      else {
        currentPitchSpeed = std::clamp(currentPitchSpeed + 2.0f * time().dt, currentPitchSpeed, basePitchSpeed);
      }
    }

    if (inputRollAcceleration != 0.0f) {
      float acceleration = inputRollAcceleration;
      if (currentRollSpeed < baseRollSpeed && inputRollAcceleration > 0.0) {
        acceleration = 8.0f;
      }
      if (currentRollSpeed > baseRollSpeed && inputRollAcceleration < 0.0) {
        acceleration = -8.0f;
      }
      currentRollSpeed += acceleration * time().dt;
    }
    else {
      if (currentRollSpeed > baseRollSpeed) {
        currentRollSpeed = std::clamp(currentRollSpeed - 8.0f * time().dt, baseRollSpeed, currentRollSpeed);
      }
      else {
        currentRollSpeed = std::clamp(currentRollSpeed + 8.0f * time().dt, currentRollSpeed, baseRollSpeed);
      }
    }

    if (inputYawAcceleration != 0.0f) {
      currentYawSpeed += inputYawAcceleration * time().dt;
    }
    else {
      if (currentYawSpeed > baseYawSpeed) {
        currentYawSpeed = std::clamp(currentYawSpeed - 0.4f * time().dt, baseYawSpeed, currentYawSpeed);
      }
      else {
        currentYawSpeed = std::clamp(currentYawSpeed + 0.4f * time().dt, currentYawSpeed, baseYawSpeed);
      }
    }

    currentPitchSpeed = std::clamp(currentPitchSpeed, -maxPitchSpeed, maxPitchSpeed);
    currentRollSpeed = std::clamp(currentRollSpeed, -maxRollSpeed, maxRollSpeed);
    currentYawSpeed = std::clamp(currentYawSpeed, -maxYawSpeed, maxYawSpeed);

    transform.rotateLocal(glm::vec3{ 1.0f, 0.0f, 0.0f }, currentPitchSpeed * time().dt);
    transform.rotateLocal(glm::vec3{ 0.0f, 0.0f, 1.0f }, currentRollSpeed * time().dt);
    transform.rotateLocal(glm::vec3{ 0.0f, 1.0f, 0.0f }, currentYawSpeed * time().dt);

    // nose direction
    {
      glm::vec3 forwardInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), forward);
      const float aspectRatio = scene().getMainCamera()->aspectRatio;
      const float fovy = scene().getMainCamera()->fov;
      forwardInViewSpace.x /= (-forwardInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
      forwardInViewSpace.y /= (-forwardInViewSpace.z * glm::sin(fovy * 0.5f));
      glm::vec2 forwardHint{};
      forwardHint.x = forwardInViewSpace.x;
      forwardHint.y = forwardInViewSpace.y;

      debug().drawScreenLine(glm::vec2{ -0.05f, 0.05f } + forwardHint, glm::vec2{ 0.0f, 0.0f } + forwardHint, { 0.0f, 0.7f, 0.0f });
      debug().drawScreenLine(glm::vec2{ 0.0f, 0.0f } + forwardHint, glm::vec2{ 0.05f, 0.05f } + forwardHint, { 0.0f, 0.7f, 0.0f });
    }

    {
      auto q = transform.rotation;
      // (heading, pitch, bank)
      // YXZ
      float headingAngle = std::atan2(2.0f * (q[0] * q[2] + q[1] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
      float pitchAngle = std::asin(2.0f * (q[2] * q[3] - q[0] * q[1]));
      float rollAngle = std::atan2(2.0f * (q[1] * q[2] + q[0] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[3] * q[3]));

      // pitch ladder
      for (int angle = -40; angle <= 40; angle += 10) {
        glm::vec2 horizonHint{};
        {
          glm::vec3 horizonInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), glm::normalize(glm::vec3{ forward.x, glm::sin(glm::radians(float(angle))), forward.z }));
          const float aspectRatio = scene().getMainCamera()->aspectRatio;
          const float fovy = scene().getMainCamera()->fov;
          horizonInViewSpace.x /= (-horizonInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
          horizonInViewSpace.y /= (-horizonInViewSpace.z * glm::sin(fovy * 0.5f));
          horizonHint.x = horizonInViewSpace.x;
          horizonHint.y = horizonInViewSpace.y;
        }

        glm::vec2 jej{ forward.x, forward.z };
        {
          glm::mat2x2 m{};
          m[0][0] = glm::cos(0.2f);
          m[0][1] = -glm::sin(0.2f);
          m[1][0] = glm::sin(0.2f);
          m[1][1] = glm::cos(0.2f);
          jej = m * jej;
        }
        glm::vec2 horizonShiftHint{};
        {
          glm::vec3 horizonShiftInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), glm::normalize(glm::vec3{ jej.x, glm::sin(glm::radians(float(angle))), jej.y }));
          const float aspectRatio = scene().getMainCamera()->aspectRatio;
          const float fovy = scene().getMainCamera()->fov;
          horizonShiftInViewSpace.x /= (-horizonShiftInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
          horizonShiftInViewSpace.y /= (-horizonShiftInViewSpace.z * glm::sin(fovy * 0.5f));
          horizonShiftHint.x = horizonShiftInViewSpace.x;
          horizonShiftHint.y = horizonShiftInViewSpace.y;
        }

        debug().drawScreenLine(horizonHint, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
        if (angle == 0) {
          debug().drawScreenLine(horizonHint + glm::vec2{ 0.0, 0.01 }, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
          debug().drawScreenLine(horizonHint - glm::vec2{ 0.0, 0.01 }, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
        }
      }

      char buf[256];
      sprintf(buf, "heading: %f", headingAngle / float(M_PI) * 180.0f);
      debug().drawScreenText({ -1.0f, 0.7f }, buf);
      sprintf(buf, "pitch: %f", pitchAngle / float(M_PI) * 180.0f);
      debug().drawScreenText({ -1.0f, 0.9f }, buf);
      sprintf(buf, "roll : %f", rollAngle / float(M_PI) * 180.0f);
      debug().drawScreenText({ -1.0f, 0.8f }, buf);
    }

    // velocity vector hint
    {
      glm::vec3 velocityInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), velocity);
      const float aspectRatio = scene().getMainCamera()->aspectRatio;
      const float fovy = scene().getMainCamera()->fov;
      velocityInViewSpace.x /= (-velocityInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
      velocityInViewSpace.y /= (-velocityInViewSpace.z * glm::sin(fovy * 0.5f));
      glm::vec2 velocityHint{};
      velocityHint.x = velocityInViewSpace.x;
      velocityHint.y = velocityInViewSpace.y;

      debug().drawScreenLine(glm::vec2{ -0.05f, 0.0f } + velocityHint, glm::vec2{ +0.05f, 0.0f } + velocityHint, glm::vec3{ 0.0f, 1.0f, 0.0f });
      debug().drawScreenLine(glm::vec2{ 0.0f, 0.0f } + velocityHint, glm::vec2{ 0.0f, 0.05f } + velocityHint, glm::vec3{ 0.0f, 1.0f, 0.0f });
    }

    char buf[256];
    sprintf(buf, "%f", glm::length(velocity));
    debug().drawScreenText({ -0.5f, 0.3f }, buf);
    sprintf(buf, "%f", velocity.x);
    debug().drawScreenText({ -0.5f, 0.2f }, buf);
    sprintf(buf, "%f", velocity.y);
    debug().drawScreenText({ -0.5f, 0.1f }, buf);
    sprintf(buf, "%f", velocity.z);
    debug().drawScreenText({ -0.5f, 0.0f }, buf);
    sprintf(buf, "%f", transform.position.y);
    debug().drawScreenText({ +0.5f, 0.3f }, buf);
  }

  float minThrust = 100.0f;
  float maxThrust = 1000.0f;
  float baseThrust = 200.0f;
  float currentThrust = 200.0f;
  float velocityShiftRate = 4.0f;
  float minSpeed = 5.0f;
  float maxSpeed = 25.0f;
  float maxPitchSpeed = 1.0f;
  float maxRollSpeed = 3.0f;
  float maxYawSpeed = 0.25f;
  float pitchAcceleration = 2.0f;
  float rollAcceleration = 5.0f;
  float yawAcceleration = 0.25f;
  float currentPitchSpeed = 0.0f;
  float currentRollSpeed = 0.0f;
  float currentYawSpeed = 0.0f;
  float targetSpeed = 20.0f;

private:
  glm::vec3 velocity{};
};

class MovingObjectScript final : public Script {
public:
  using Script::Script;

  void initialize() override {
    m_chasedObject = &scene().getMainCamera()->object;
    m_forwardRotationSpeed = random().next(-1.0f, 1.0f);
    m_rightRotationSpeed = random().next(-1.0f, 1.0f);
    transform.position.x += random().next(-20.0f, 20.0f);
    transform.position.y += 0.1f * random().next(-10.0f, 10.0f);
    transform.position.z += random().next(-20.0f, 20.0f);
  }

  void update() override {
    transform.rotateLocal(transform.forward(), m_forwardRotationSpeed * time().dt);
    transform.rotateLocal(transform.right(), m_rightRotationSpeed * time().dt);
    glm::vec3 delta = m_chasedObject->transform.position - transform.position;
    glm::vec3 dir = glm::normalize(delta);
    if (input().keyPressed[SDL_SCANCODE_E]) {
      dir *= -10000.0f * (1.0f / (1.0f + glm::l2Norm(delta)));
    }
    m_velocity += dir * time().dt;
    transform.position += m_velocity * time().dt;
  }

  Object* m_chasedObject = nullptr;
  float m_forwardRotationSpeed = 1.0f;
  float m_rightRotationSpeed = 1.0f;
  glm::vec3 m_velocity{};
};

class MovingObjectGeneratorScript final : public Script {
public:
  using Script::Script;

  void update() override {
    if (input().keyPressed[SDL_SCANCODE_SPACE]) {
      Object* newObject = scene().makeObject();
      newObject->addComponent<MovingObjectScript>();
      {
        MeshRenderer* meshRenderer = newObject->addComponent<MeshRenderer>();
        meshRenderer->mesh = resources().get<Mesh>(SID("object"));
        meshRenderer->materials = { resources().get<Material>(SID("default.object")) };
      }
      generatedObjects.push_back(newObject);
      Object* newObjectParent = scene().makeObject();
      newObjectParent->addComponent<MovingObjectScript>();
      {
        MeshRenderer* meshRenderer = newObjectParent->addComponent<MeshRenderer>();
        meshRenderer->mesh = resources().get<Mesh>(SID("object"));
        meshRenderer->materials = { resources().get<Material>(SID("default.object")) };
      }
      newObjectParent->addChild(newObject);
      generatedObjects.push_back(newObjectParent);
    }
    if (input().keyPressed[SDL_SCANCODE_P]) {
      for (Object*& object : generatedObjects) {
        if (random().next(0.0f, 1.0f) > 0.5f) {
          object->destroy();
          object = nullptr;
        }
      }
      generatedObjects.erase(std::remove_if(generatedObjects.begin(), generatedObjects.end(), [](auto& ptr) { return ptr == nullptr; }), generatedObjects.end());
    }
  }

private:
  std::vector<Object*> generatedObjects;
};

class Application {
public:
  ~Application() {
    shutDown();
  }

  int setUp() {
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
      return 1;
    if (IMG_Init(IMG_INIT_PNG) == 0)
      return 1;

    m_window = SDL_CreateWindow("",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      800, 600,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window)
      return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context)
      return 1;

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
      return 1;

    return 0;
  }

  void shutDown() {
    if (m_shutdownCalled)
      return;

    m_shutdownCalled = true;

    if (m_context) {
      SDL_GL_DeleteContext(m_context);
      m_context = nullptr;
    }

    if (m_window) {
      SDL_DestroyWindow(m_window);
      m_window = nullptr;
    }

    IMG_Quit();
    SDL_Quit();
  }

  static void glDebugCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* user_param) {
    printf("gl error: %s\n", msg);
  }

  void importMesh(const char* filename, StringID sid) {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t submeshCount;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> submeshes;
    {
      FILE* file = fopen("su37.meshresource", "rb");
      if (!file)
        throw std::runtime_error("file not found");

      if (fread(&vertexCount, sizeof(vertexCount), 1, file) != 1)
        throw std::runtime_error("import error");

      vertices.resize(vertexCount * 6);
      if (fread(vertices.data(), 6 * sizeof(float), vertexCount, file) != vertexCount)
        throw std::runtime_error("import error");

      if (fread(&indexCount, sizeof(indexCount), 1, file) != 1)
        throw std::runtime_error("import error");

      indices.resize(indexCount);
      if (fread(indices.data(), sizeof(uint32_t), indexCount, file) != indexCount)
        throw std::runtime_error("import error");

      if (fread(&submeshCount, sizeof(submeshCount), 1, file) != 1)
        throw std::runtime_error("import error");

      submeshes.resize(2 * submeshCount);
      if (fread(submeshes.data(), 2 * sizeof(uint32_t), submeshCount, file) != submeshCount)
        throw std::runtime_error("import error");

      fclose(file);
    }

    auto mesh = m_resources.create<Mesh>(sid);

    Mesh::Options options;
    options.attributes = {
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
    };
    options.indexBufferData = indices.data();
    options.indexCount = indexCount;
    options.indexFormat = Mesh::IndexFormat::u32;
    options.vertexBufferData = vertices.data();
    options.vertexCount = vertexCount;
    mesh->initialize(options);

    mesh->submeshes.resize(submeshCount);
    for (uint32_t i = 0; i < submeshCount; i++) {
      mesh->submeshes[i] = { submeshes[2 * i + 0], submeshes[2 * i + 1] };
    }
  }

  void setUpResources() {
    importMesh("su37.meshresource", SID("su37"));

    {
      auto mesh = m_resources.create<Mesh>(SID("land"));
      Vertex vertices[]{
        Vertex{ { -10000.0f, 0.0f, -10000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { 10000.0f, 0.0f, -10000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { 10000.0f, 0.0f, 10000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { -10000.0f, 0.0f, 10000.0f }, { 0.0f, 1.0f, 0.0f } },
      };
      uint32_t indices[]{ 0, 2, 1, 0, 3, 2 };

      Mesh::Options options;
      options.attributes = {
        Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
        Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
      };
      options.indexBufferData = indices;
      options.indexCount = 6;
      options.indexFormat = Mesh::IndexFormat::u32;
      options.vertexBufferData = vertices;
      options.vertexCount = 4;
      mesh->initialize(options);
    }

    {
      auto mesh = m_resources.create<Mesh>(SID("object"));
      Vertex vertices[]{
        Vertex{ { -1.0, -1.0, 0.0 }, { 1.0, 0.0, 0.0 } },
        Vertex{ { 1.0, -1.0, 0.0 }, { 0.0, 1.0, 0.0 } },
        Vertex{ { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } }
      };
      uint32_t indices[]{ 0, 1, 2 };

      Mesh::Options options;
      options.attributes = {
        Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
        Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
      };
      options.indexBufferData = indices;
      options.indexCount = 3;
      options.indexFormat = Mesh::IndexFormat::u32;
      options.vertexBufferData = vertices;
      options.vertexCount = 3;
      mesh->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("default"));
      Shader::Options options{};
      options.vertexSource = "layout (location = 0) in vec3 inPosition;\n"
                             "layout (location = 1) in vec3 inNormal;\n"
                             "layout (location = 0) out vec3 position;\n"
                             "layout (location = 1) out vec3 normal;\n"
                             "layout (std140, binding = 2) uniform Material {\n"
                             "  vec3 color;\n"
                             "  vec3 ambient;\n"
                             "} material;\n"
                             "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                             "uniform mat4 model;"
                             "void main() {\n"
                             "  gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
                             "  position = (model * vec4(inPosition, 1.0)).xyz;\n"
                             "  normal = normalize(inNormal);\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec3 position;\n"
                               "layout (location = 1) in vec3 inNormal;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "struct Light { vec3 position; vec3 color; };\n"
                               "layout (std140, binding = 1) uniform Lights {\n"
                               "  vec3 positions[128];\n"
                               "  vec3 colors[128];\n"
                               "  int count;\n"
                               "} lights;\n"
                               "layout (std140, binding = 2) uniform Material {\n"
                               "  vec3 color;\n"
                               "  vec3 ambient;\n"
                               "} material;\n"
                               "uniform float time;"
                               "void main() {\n"
                               "  vec3 lightsColor = vec3(0.0,0.0,0.0);\n"
                               "  vec3 normal = normalize(inNormal);\n"
                               "  for (int i = 0; i < lights.count; i++) { lightsColor += lights.colors[i] * max(0.0, dot(normal, normalize(lights.positions[i] - position))); }"
                               "  fragColor = vec4(material.color * (material.ambient + lightsColor), 1.0);\n"
                               "}";
      shader->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("default.land"));
      Shader::Options options{};
      options.vertexSource = "layout (location = 0) in vec3 inPosition;\n"
                             "layout (location = 1) in vec3 inNormal;\n"
                             "layout (location = 0) out vec3 position;\n"
                             "layout (location = 1) out vec3 normal;\n"
                             "layout (std140, binding = 2) uniform Material {\n"
                             "  vec3 color;\n"
                             "  vec3 ambient;\n"
                             "} material;\n"
                             "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                             "uniform mat4 model;"
                             "void main() {\n"
                             "  gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
                             "  position = (model * vec4(inPosition, 1.0)).xyz;\n"
                             "  normal = normalize(inNormal);\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec3 position;\n"
                               "layout (location = 1) in vec3 inNormal;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "struct Light { vec3 position; vec3 color; };\n"
                               "layout (std140, binding = 1) uniform Lights {\n"
                               "  vec3 positions[128];\n"
                               "  vec3 colors[128];\n"
                               "  int count;\n"
                               "} lights;\n"
                               "layout (std140, binding = 2) uniform Material {\n"
                               "  vec3 color;\n"
                               "  vec3 ambient;\n"
                               "} material;\n"
                               "uniform float time;"
                               "void main() {\n"
                               "  vec3 lightsColor = vec3(0.0,0.0,0.0);\n"
                               "  vec3 normal = normalize(inNormal);\n"
                               "  for (int i = 0; i < lights.count; i++) { lightsColor += lights.colors[i] * max(0.0, dot(normal, normalize(lights.positions[i] - position))); }"
                               "  vec3 materialColor = material.color;\n"
                               "  if ((int(0.05 * position.x) + int(0.05 * position.z)) % 2 == 0) materialColor *= 0.5;"
                               "  fragColor = vec4(materialColor * (material.ambient + lightsColor), 1.0);\n"
                               "}";
      shader->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("screenspace.sky"));
      Shader::Options options{};
      options.vertexSource = "layout (location = 0) out vec2 uv;\n"
                             "void main() {\n"
                             "  const vec2 verts[] = vec2[](vec2(-1.0, 1.0), vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));"
                             "  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);\n"
                             "  uv = verts[gl_VertexID];\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec2 uv;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "uniform vec3 cameraPosition;\n"
                               "uniform vec3 cameraUp;\n"
                               "uniform vec3 cameraForward;\n"
                               "uniform vec3 cameraRight;\n"
                               "uniform float aspectRatio;\n"
                               "uniform float fov;\n"
                               "vec3 skyColor(vec3 rayOrig, vec3 rayDir) {\n"
                               "  if (rayDir.y < 0.0) return vec3(0.0, 0.0, 0.0);\n"
                               "  return mix(vec3(0.9, 0.9, 0.9), vec3(0.1, 0.2, 0.5), sqrt(sqrt(rayDir.y)));"
                               "}\n"
                               "void main() {\n"
                               "  vec3 rayOrig = cameraPosition;\n"
                               "  vec3 rayDir = normalize(cameraForward + sin(0.5 * fov) * cameraUp * uv.y + cameraRight * uv.x * aspectRatio * sin(0.5 * fov));"
                               "  fragColor = vec4(skyColor(rayOrig, rayDir), 1.0);"
                               "}";
      shader->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("debug.drawline"));
      Shader::Options options{};
      options.vertexSource = "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                             "uniform vec3 verts[2];\n"
                             "uniform vec3 color;\n"
                             "layout (location = 0) out vec3 outColor;\n"
                             "void main() {\n"
                             "  gl_Position = projection * view * vec4(verts[gl_VertexID], 1.0);\n"
                             "  outColor = color;\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec3 color;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "void main() {\n"
                               "  fragColor = vec4(color, 1.0);\n"
                               "}";
      shader->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("debug.drawscreenline"));
      Shader::Options options{};
      options.vertexSource = "uniform vec2 verts[2];\n"
                             "uniform vec3 color;\n"
                             "layout (location = 0) out vec3 outColor;\n"
                             "void main() {\n"
                             "  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);\n"
                             "  outColor = color;\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec3 color;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "void main() {\n"
                               "  fragColor = vec4(color, 1.0);\n"
                               "}";
      shader->initialize(options);
    }

    {
      auto shader = m_resources.create<Shader>(SID("debug.drawscreentext"));
      Shader::Options options{};
      options.vertexSource = "layout (location = 0) out vec2 outUV;\n"
                             "uniform vec2 screenPosition;\n"
                             "uniform vec2 screenCharSize;\n"
                             "uniform vec2 charBottomLeft;\n"
                             "uniform vec2 charSize;\n"
                             "void main() {\n"
                             "  const vec2 verts[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));\n"
                             "  gl_Position = vec4(screenPosition + screenCharSize * verts[gl_VertexID], 0.0, 1.0);\n"
                             "  outUV = charBottomLeft + charSize * verts[gl_VertexID];\n"
                             "}";
      options.fragmentSource = "layout (location = 0) in vec2 uv;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "layout (binding = 0) uniform sampler2D textureFont;\n"
                               "void main() {\n"
                               "  vec3 color = texture(textureFont, uv).xyz;"
                               "  if (color.y < 0.5) discard;\n"
                               "  fragColor = vec4(texture(textureFont, uv).xyz, 1.0);\n"
                               "}";
      shader->initialize(options);
    }

    {
      auto material = m_resources.create<Material>(SID("default.land"));
      material->initialize(m_resources.get<Shader>(SID("default.land")));
      material->setValue("Material.color", glm::vec3{ 0.4f, 0.2f, 0.1f });
    }

    {
      auto material = m_resources.create<Material>(SID("default.object"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 1.0f, 1.0f, 1.0f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.body"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.2f, 0.400000f, 0.200000f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.cockpit"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.274425f, 0.282128f, 0.800000f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.engine"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.100000f, 0.100000f, 0.100000f });
    }

    {
      auto material = m_resources.create<Material>(SID("sky"));
      material->initialize(m_resources.get<Shader>(SID("screenspace.sky")));
    }

    {
      Texture2D::Options options;
      options.path = "./debug.font.png";
      options.magFilter = GL_NEAREST;
      options.minFilter = GL_NEAREST;
      auto texture = m_resources.create<Texture2D>(SID("debug.font"));
      texture->initialize(options);
    }
  }

  void setUpScene() {
    Object* movingObjectGeneratorObject = m_scene.makeObject();
    movingObjectGeneratorObject->addComponent<MovingObjectGeneratorScript>();

    Object* landObject = m_scene.makeObject();
    landObject->transform.position = { 0.0f, -10.0f, 0.0f };
    MeshRenderer* landObjectMeshRenderer = landObject->addComponent<MeshRenderer>();
    landObjectMeshRenderer->mesh = m_resources.get<Mesh>(SID("land"));
    landObjectMeshRenderer->materials.push_back(m_resources.get<Material>(SID("default.land")));

    Object* lights[2];
    lights[0] = m_scene.makeObject();
    lights[0]->transform.position = { 0.0, 5.0f, 0.0f };
    lights[0]->addComponent<Light>()->color = { 5.0f, 5.0f, 5.0f };
    lights[1] = m_scene.makeObject();
    lights[1]->transform.position = { 2.0, 2.0f, 0.0f };
    lights[1]->addComponent<Light>()->color = { 3.0f, 0.0f, 0.0f };

    Object* plane = m_scene.makeObject();
    {
      plane->tag = 0;
      plane->transform.position = { 0.0f, 10.0f, 0.0f };
      MeshRenderer* meshRenderer = plane->addComponent<MeshRenderer>();
      meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
      plane->addComponent<PlaneControlScript>();

      Object* childLight = m_scene.makeObject();
      plane->addChild(childLight);
      Light* light = childLight->addComponent<Light>();
      childLight->transform.position = { 0.0f, 0.0f, 4.0f };
      light->color = { 5.0f, 5.0f, 5.0f };
    }

    Object* plane1 = m_scene.makeObject();
    {
      plane1->transform.position = { 0.0f, 10.0f, 0.0f };
      MeshRenderer* meshRenderer = plane1->addComponent<MeshRenderer>();
      meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
    }

    Object* mainCameraObject = m_scene.makeObject();
    {
      Camera* mainCamera = mainCameraObject->addComponent<Camera>();
      mainCamera->aspectRatio = float(m_width) / float(m_height);
      mainCamera->isMain = true;
      mainCamera->fov = glm::radians(70.0f);
      mainCameraObject->addComponent<PlaneChaseCameraScript>();
    }
  }

  void run() {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugCallback, this);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

    m_scene.scriptContext.input = &m_input;
    m_scene.scriptContext.random = &m_random;
    m_scene.scriptContext.time = &m_time;
    m_scene.scriptContext.scene = &m_scene;
    m_scene.scriptContext.resources = &m_resources;
    m_scene.scriptContext.debug = &m_debug;
    SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);

    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    GLuint color, depth, fbo;

    glCreateTextures(GL_TEXTURE_2D, 1, &color);
    glTextureStorage2D(color, 1, GL_SRGB8_ALPHA8, m_width, m_height);

    glCreateTextures(GL_TEXTURE_2D, 1, &depth);
    glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, m_width, m_height);

    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
    glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
    }

    setUpResources();
    setUpScene();

    GLint ubOffsetAlignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ubOffsetAlignment);

    struct MaterialUniformBufferStackAllocator {
    public:
      MaterialUniformBufferStackAllocator(uint32_t size, uint32_t alignment) : alignment(alignment), backingStorage(size) {}

      struct PtrOffsetPair {
        char* ptr;
        uint32_t offset;
      } alloc(uint32_t size) {
        uint32_t ret = nextAllocationStart;
        m_allocatedSize = nextAllocationStart + size;
        nextAllocationStart = (m_allocatedSize + alignment - 1) & (~(alignment - 1));
        return { &backingStorage[ret], ret };
      }

      void clear() {
        nextAllocationStart = 0;
      }

      uint32_t allocatedSize() const {
        return m_allocatedSize;
      }

      void* data() {
        return backingStorage.data();
      }

    private:
      uint32_t alignment;
      std::vector<char> backingStorage;
      uint32_t nextAllocationStart = 0;
      uint32_t m_allocatedSize = 0;
    } uboMaterialAllocator(64 * 1024, ubOffsetAlignment);

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    const uint32_t FRAME_0 = 0;
    const uint32_t FRAME_1 = 1;
    GLuint uniformBuffers[MAX_FRAMES_IN_FLIGHT][2];
    GLuint uboScene[MAX_FRAMES_IN_FLIGHT];
    GLuint uboMaterial[MAX_FRAMES_IN_FLIGHT];
    for (uint32_t frameIdx = 0; frameIdx < MAX_FRAMES_IN_FLIGHT; frameIdx++) {
      glCreateBuffers(2, uniformBuffers[frameIdx]);
      glNamedBufferData(uniformBuffers[frameIdx][0], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
      glNamedBufferData(uniformBuffers[frameIdx][1], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
      uboScene[frameIdx] = uniformBuffers[frameIdx][0];
      uboMaterial[frameIdx] = uniformBuffers[frameIdx][1];
    }
    GLuint vaoDebug;
    glCreateVertexArrays(1, &vaoDebug);

    SDL_GL_SetSwapInterval(0);
    SDL_ShowWindow(m_window);
    uint32_t currentFrame = 0;
    auto advanceToNextFrame = [&MAX_FRAMES_IN_FLIGHT, &currentFrame]() {
      currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    };

    SDL_bool relativeMouseMode = SDL_TRUE;
    SDL_SetRelativeMouseMode(relativeMouseMode);
    m_time.timeSinceStart = 0.0;
    uint64_t ticksLast = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    int counter = 0;
    float frameTimeAccum = 0.0f;
    while (true) {
      handleEvents();
      if (m_quit)
        break;

      uint64_t ticksNow = SDL_GetPerformanceCounter();
      uint64_t ticksDiff = ticksNow - ticksLast;
      double delta = double(ticksDiff) / double(frequency);
      m_time.timeSinceStart += delta;
      m_time.dt = float(delta);

      {
        counter++;
        frameTimeAccum += m_time.dt / float(60.0f);
        if (counter >= 60) {
          counter = 0;
          char title[1024];
          sprintf(title, "%f ms, %f FPS", 1000.0f * frameTimeAccum, (1.0f / frameTimeAccum));
          frameTimeAccum = 0.0f;
          SDL_SetWindowTitle(m_window, title);
        }
      }
      ticksLast = ticksNow;

      // Update
      {
        if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyPressed[SDL_SCANCODE_C]) {
          relativeMouseMode = relativeMouseMode == SDL_TRUE ? SDL_FALSE : SDL_TRUE;
          SDL_SetRelativeMouseMode(relativeMouseMode);
        }

        for (auto& script : m_scene.scripts.activeScripts) {
          script->update();
        }
        for (auto& script : m_scene.scripts.pendingScripts) {
          script->initialize();
          m_scene.scripts.activeScripts.push_back(std::move(script));
        }
        m_scene.scripts.pendingScripts.clear();

        m_scene.destroyObjects();
      }

      // Render

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glClearDepthf(0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_GREATER);

      struct SceneUniformBufferLayout {
        struct {
          glm::mat4 view;
          glm::mat4 projection;
        } matrices;
        char pad0[128];
        struct Lights {
          struct {
            glm::vec3 v;
            char pad[4];
          } positions[128];
          struct {
            glm::vec3 v;
            char pad[4];
          } colors[128];
          int32_t count;
          char pad[12];
        } lights;
      } sceneUBOLayout;

      // Set up scene uniform
      {
        sceneUBOLayout.matrices.view = m_scene.getMainCamera()->viewMatrix4();
        sceneUBOLayout.matrices.projection = m_scene.getMainCamera()->projectionMatrix4();
        uint32_t lightId = 0;
        for (auto& light : m_scene.components.lights) {
          sceneUBOLayout.lights.positions[lightId].v = light->transform.worldPosition();
          sceneUBOLayout.lights.colors[lightId].v = light->color;
          lightId++;
        }
        sceneUBOLayout.lights.count = lightId;
        glNamedBufferSubData(uboScene[currentFrame], 0, sizeof(sceneUBOLayout), &sceneUBOLayout);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboScene[currentFrame], offsetof(SceneUniformBufferLayout, matrices), sizeof(sceneUBOLayout.matrices));
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboScene[currentFrame], offsetof(SceneUniformBufferLayout, lights), sizeof(sceneUBOLayout.lights));
      }

      // Set up materials buffer
      std::unordered_map<const Material*, uint32_t> offsetForMaterial;
      uboMaterialAllocator.clear();
      {
        for (auto& meshRenderer : m_scene.components.meshRenderers) {
          for (std::size_t i = 0; i < meshRenderer->mesh->submeshes.size(); i++) {
            const Material* material = meshRenderer->materials[i];
            if (offsetForMaterial.find(material) == offsetForMaterial.end()) {
              auto [ptr, offset] = uboMaterialAllocator.alloc(material->materialUniformBlock->size);
              offsetForMaterial[material] = offset;
              memcpy(ptr, material->uniformStorage.get(), material->materialUniformBlock->size);
            }
          }
        }

        glNamedBufferSubData(uboMaterial[currentFrame], 0, uboMaterialAllocator.allocatedSize(), uboMaterialAllocator.data());
      }

      for (auto& meshRenderer : m_scene.components.meshRenderers) {
        glBindVertexArray(meshRenderer->mesh->vao);
        for (std::size_t i = 0; i < meshRenderer->mesh->submeshes.size(); i++) {
          Mesh::SubMesh& submesh = meshRenderer->mesh->submeshes[i];
          const Material* material = meshRenderer->materials[i];
          glBindProgramPipeline(material->shader->programPipeline);
          glBindBufferRange(GL_UNIFORM_BUFFER, 2, uboMaterial[currentFrame], offsetForMaterial.at(material), material->materialUniformBlock->size);
          material->shader->vertShader.setUniform(SID("model"), meshRenderer->transform.asMatrix4());
          material->shader->vertShader.setUniform(SID("time"), float(m_time.timeSinceStart));
          material->shader->fragShader.setUniform(SID("time"), float(m_time.timeSinceStart));
          glDrawElements(GL_TRIANGLES, submesh.indexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(submesh.indexStart * sizeof(uint32_t)));
        }
      }

      // screenspace effect, post-render, specific depth func
      {
        glDepthFunc(GL_EQUAL);
        glBindVertexArray(vaoDebug);
        const Material* material = m_resources.get<Material>(SID("sky"));
        glBindProgramPipeline(material->shader->programPipeline);
        material->shader->fragShader.setUniform(SID("cameraPosition"), m_scene.getMainCamera()->transform.position);
        material->shader->fragShader.setUniform(SID("cameraForward"), m_scene.getMainCamera()->transform.forward());
        material->shader->fragShader.setUniform(SID("cameraUp"), m_scene.getMainCamera()->transform.up());
        material->shader->fragShader.setUniform(SID("cameraRight"), m_scene.getMainCamera()->transform.right());
        material->shader->fragShader.setUniform(SID("aspectRatio"), m_scene.getMainCamera()->aspectRatio);
        material->shader->fragShader.setUniform(SID("fov"), m_scene.getMainCamera()->fov);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDepthFunc(GL_GREATER);
      }

      glDisable(GL_DEPTH_TEST);
      {
        Shader* lineShader = m_resources.get<Shader>(SID("debug.drawline"));
        glBindVertexArray(vaoDebug);
        glBindProgramPipeline(lineShader->programPipeline);
        for (auto& command : m_debug.m_drawLineCommands) {
          lineShader->vertShader.setUniformArray(SID("verts[0]"), 2, command.verts);
          lineShader->vertShader.setUniform(SID("color"), command.color);
          glDrawArrays(GL_LINES, 0, 2);
        }
      }

      {
        Shader* lineShader = m_resources.get<Shader>(SID("debug.drawscreenline"));
        glBindVertexArray(vaoDebug);
        glBindProgramPipeline(lineShader->programPipeline);
        for (auto& command : m_debug.m_drawScreenLineCommands) {
          lineShader->vertShader.setUniformArray(SID("verts[0]"), 2, command.verts);
          lineShader->vertShader.setUniform(SID("color"), command.color);
          glDrawArrays(GL_LINES, 0, 2);
        }
      }

      {
        Shader* screenTextShader = m_resources.get<Shader>(SID("debug.drawscreentext"));
        Texture2D* textTexture = m_resources.get<Texture2D>(SID("debug.font"));
        glBindVertexArray(vaoDebug);
        glBindProgramPipeline(screenTextShader->programPipeline);
        glBindTextures(0, 1, &textTexture->handle);

        const float charTextureWidth = 1.0f / 16.0f;
        const float charTextureHeight = 1.0f / 16.0f;
        const float charScreenWidth = 1.0f / 20.0f * (float(textTexture->width) / float(textTexture->height));
        const float charScreenHeight = 1.0f / 20.0f;

        for (auto& command : m_debug.m_drawScreenTextCommands) {
          float x = command.topleft.x;
          float y = command.topleft.y;
          for (const char* c = command.str.c_str(); *c != 0; c++) {
            uint32_t cx = *c % 16;
            uint32_t cy = 15 - *c / 16;
            screenTextShader->vertShader.setUniform(SID("screenPosition"), glm::vec2(x, y));
            screenTextShader->vertShader.setUniform(SID("screenCharSize"), glm::vec2(charScreenWidth, charScreenHeight));
            screenTextShader->vertShader.setUniform(SID("charBottomLeft"), glm::vec2(cx * charTextureWidth, cy * charTextureHeight));
            screenTextShader->vertShader.setUniform(SID("charSize"), glm::vec2(charTextureWidth, charTextureHeight));
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += charScreenWidth;
          }
        }
      }

      glBindVertexArray(0);
      glBindProgramPipeline(0);

      m_debug.clear();

      {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
        glBlitFramebuffer(
          0, 0, m_width, m_height,
          0, 0, m_width, m_height,
          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }

      SDL_GL_SwapWindow(m_window);
      advanceToNextFrame();
    }
  }

private:
  void handleEvents() {
    m_input.mousedx = 0.0;
    m_input.mousedy = 0.0;
    for (int scancode = 0; scancode < SDL_NUM_SCANCODES; scancode++) {
      m_input.keyPressed[scancode] = false;
      m_input.keyReleased[scancode] = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          m_quit = true;
          return;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          if (event.key.repeat == 0) {
            auto scancode = event.key.keysym.scancode;
            m_input.prevKeyDown[scancode] = m_input.keyDown[scancode];
            m_input.keyDown[scancode] = event.key.state == SDL_PRESSED;
            m_input.keyPressed[scancode] = event.key.state == SDL_PRESSED;
            m_input.keyReleased[scancode] = event.key.state == SDL_RELEASED;
          }
          break;
        }
        case SDL_MOUSEMOTION: {
          m_input.mousedx = float(event.motion.xrel);
          m_input.mousedy = float(event.motion.yrel);
          break;
        }
        case SDL_WINDOWEVENT: {
          switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED: {
              SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
              glViewport(0, 0, m_width, m_height);
            }
          }
        }
        default:
          break;
      }
    }
  }

  SDL_Window* m_window = nullptr;
  SDL_GLContext m_context = nullptr;
  bool m_quit = false;
  bool m_shutdownCalled = false;
  int m_width, m_height;
  Input m_input;
  Time m_time;
  Scene m_scene;
  Random m_random;
  Resources m_resources;
  Debug m_debug;
};

int main(int, char**) {
  Application app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
