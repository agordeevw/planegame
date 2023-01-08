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
    return glm::perspective(fov, aspectRatio, 0.01f, 1000.0f);
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

class Script : public Component {
public:
  Script(Object& object)
    : Component(object) {}
  virtual ~Script() = default;

  virtual void initialize() {}
  virtual void update() = 0;

  Scene& scene() { return *m_context.scene; }
  const Input& input() { return *m_context.input; }
  const Time& time() { return *m_context.time; }
  Random& random() { return *m_context.random; }
  Resources& resources() { return *m_context.resources; }
  Debug& debug() { return *m_context.debug; }

private:
  friend class Scene;

  ScriptContext m_context;
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
    GLuint binding;
    GLuint size;
    GLuint index;
    std::vector<Entry> entries;
  };

  struct Uniform {
    std::string name;
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

  const UniformBlock* getUniformBlock(const char* name) const {
    auto it = std::find_if(uniformBlocks.begin(), uniformBlocks.end(), [name](const UniformBlock& block) { return block.name == name; });
    if (it != uniformBlocks.end())
      return &(*it);
    else
      return nullptr;
  }

  const Uniform* getUniform(const char* name) const {
    auto it = std::find_if(uniforms.begin(), uniforms.end(), [name](const Uniform& block) { return block.name == name; });
    if (it != uniforms.end())
      return &(*it);
    else
      return nullptr;
  }

  template <class T>
  void setUniform(const char* uniformName, const T& value) {
    const Uniform* pUniform = getUniform(uniformName);
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
  void setUniformArray(const char* uniformName, uint32_t count, const T* value) {
    const Uniform* pUniform = getUniform(uniformName);
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
        entry.type = values[0];
        entry.size = values[1];
        entry.offset = values[2];
        entry.stride = values[4];
        uniformBlocks[blockIndex].entries.push_back(std::move(entry));
      }
      else {
        Uniform uniform;
        uniform.name = uniformNameBuffer;
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
    SDL_FreeSurface(surf);
  }

  uint32_t width = -1;
  uint32_t height = -1;
  GLuint handle = -1;
};

struct Material {
  void initialize(Shader* shader) {
    this->shader = shader;
    materialUniformBlock = shader->fragShader.getUniformBlock("Material");
    uniformStorage = std::make_unique<char[]>(materialUniformBlock->size);
  }

  template <class T>
  void setIndexedValue(const char* name, int index, const T& value) {
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

class Resources {
public:
  template <class T, class... Args>
  T* create(std::string name, Args&&... args) {
    auto res = std::make_unique<T>(std::forward<Args>(args)...);
    T* ret = res.get();
    auto addResource = [&name, &res](auto& resourceVector, std::unordered_map<std::string, std::size_t>& mapNameToResourceIdx) {
      mapNameToResourceIdx[std::move(name)] = resourceVector.size();
      resourceVector.push_back(std::move(res));
    };

    if constexpr (std::is_same_v<T, Mesh>) {
      addResource(meshes, nameToMeshIdx);
    }
    else if constexpr (std::is_same_v<T, Shader>) {
      addResource(shaders, nameToShaderIdx);
    }
    else if constexpr (std::is_same_v<T, Material>) {
      addResource(materials, nameToMaterialIdx);
    }
    else if constexpr (std::is_same_v<T, Texture2D>) {
      addResource(textures2D, nameToTexture2DIdx);
    }
    else {
      static_assert(false, "Resource type not handled");
    }

    return ret;
  }

  template <class T>
  T* get(const std::string& name) {
    auto getResource = [&name](auto& resourceVector, std::unordered_map<std::string, std::size_t>& mapNameToResourceIdx) {
      return resourceVector[mapNameToResourceIdx.at(name)].get();
    };

    T* ret;
    if constexpr (std::is_same_v<T, Mesh>) {
      ret = getResource(meshes, nameToMeshIdx);
    }
    else if constexpr (std::is_same_v<T, Shader>) {
      ret = getResource(shaders, nameToShaderIdx);
    }
    else if constexpr (std::is_same_v<T, Material>) {
      ret = getResource(materials, nameToMaterialIdx);
    }
    else if constexpr (std::is_same_v<T, Texture2D>) {
      ret = getResource(textures2D, nameToTexture2DIdx);
    }
    else {
      static_assert(false, "Resource type not handled");
    }
    return ret;
  }

private:
  std::vector<std::unique_ptr<Mesh>> meshes;
  std::vector<std::unique_ptr<Shader>> shaders;
  std::vector<std::unique_ptr<Material>> materials;
  std::vector<std::unique_ptr<Texture2D>> textures2D;
  std::unordered_map<std::string, std::size_t> nameToMeshIdx;
  std::unordered_map<std::string, std::size_t> nameToShaderIdx;
  std::unordered_map<std::string, std::size_t> nameToMaterialIdx;
  std::unordered_map<std::string, std::size_t> nameToTexture2DIdx;
};

class Debug {
public:
  struct DrawLineCommand {
    glm::vec3 verts[2];
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

  void drawScreenText(glm::vec2 topleft, const char* str) {
    DrawScreenTextCommand command;
    command.topleft = topleft;
    command.str = str;
    m_drawScreenTextCommands.push_back(command);
  }

  void clear() {
    m_drawLineCommands.clear();
    m_drawScreenTextCommands.clear();
  }

private:
  friend class Application;

  std::vector<DrawLineCommand> m_drawLineCommands;
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
    if (input().keyDown[SDL_SCANCODE_LALT]) {
      horizAngleOffset += input().mousedx * time().dt;
    }
    else {
      horizAngleOffset = 0.0f;
    }

    glm::vec3 offset = -5.0f * forward + 1.0f * up;
    offset = glm::rotate(glm::angleAxis(horizAngleOffset, up), offset);
    transform.position = m_chasedObject->transform.position + offset;
    transform.rotation = glm::quatLookAt(glm::normalize(m_chasedObject->transform.position + 1.0f * up - transform.position), m_chasedObject->transform.up());
  }

  float horizAngleOffset = 0.0f;
  Object* m_chasedObject = nullptr;
};

class PlaneControlScript final : public Script {
public:
  using Script::Script;

  void initialize() override {
    velocity = 20.0f * transform.forward();
  }

  void update() override {
    Transform& transform = object.transform;

    if (input().keyDown[SDL_SCANCODE_W]) {
      transform.rotateLocal(glm::vec3{ 1.0f, 0.0f, 0.0f }, -pitchSpeed * time().dt);
    }
    if (input().keyDown[SDL_SCANCODE_S]) {
      transform.rotateLocal(glm::vec3{ 1.0f, 0.0f, 0.0f }, +pitchSpeed * time().dt);
    }
    if (input().keyDown[SDL_SCANCODE_D]) {
      transform.rotateLocal(glm::vec3{ 0.0f, 0.0f, 1.0f }, -rollSpeed * time().dt);
    }
    if (input().keyDown[SDL_SCANCODE_A]) {
      transform.rotateLocal(glm::vec3{ 0.0f, 0.0f, 1.0f }, +rollSpeed * time().dt);
    }
    if (input().keyDown[SDL_SCANCODE_E]) {
      transform.rotateLocal(glm::vec3{ 0.0f, 1.0f, 0.0f }, -yawSpeed * time().dt);
    }
    if (input().keyDown[SDL_SCANCODE_Q]) {
      transform.rotateLocal(glm::vec3{ 0.0f, 1.0f, 0.0f }, +yawSpeed * time().dt);
    }

    glm::vec3 forward = transform.forward();
    glm::vec3 up = transform.up();

    float thrust = 10.0f;
    float drag = (0.2f + 0.25f * std::abs(glm::dot(up, glm::normalize(velocity)))) * l2Norm(velocity);
    float weight = 5.0f;
    float lift = 0.25f * l2Norm(velocity);

    glm::vec3 accel{};
    velocity += (thrust * forward + lift * up - drag * glm::normalize(velocity) + weight * glm::vec3(0.0f, -1.0f, 0.0f)) * time().dt;
    transform.position += velocity * time().dt;

    debug().drawLine(transform.position, transform.position + velocity, glm::vec3{ 1.0f, 1.0f, 1.0f });
    debug().drawLine(transform.position, transform.position + 5.0f * glm::vec3{ 0.0f, 0.0f, -1.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
    char speed[256];
    sprintf(speed, "%f", glm::length(velocity));
    debug().drawScreenText({ -0.5f, 0.0f }, speed);
    sprintf(speed, "%f", transform.position.y);
    debug().drawScreenText({ +0.5f, 0.0f }, speed);
  }

  float minSpeed = 5.0f;
  float pitchSpeed = 1.0f;
  float rollSpeed = 2.0f;
  float yawSpeed = 0.25f;

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
        meshRenderer->mesh = resources().get<Mesh>("object");
        meshRenderer->materials = { resources().get<Material>("default.object") };
      }
      generatedObjects.push_back(newObject);
      Object* newObjectParent = scene().makeObject();
      newObjectParent->addComponent<MovingObjectScript>();
      {
        MeshRenderer* meshRenderer = newObjectParent->addComponent<MeshRenderer>();
        meshRenderer->mesh = resources().get<Mesh>("object");
        meshRenderer->materials = { resources().get<Material>("default.object") };
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

  void setUpResources() {
    {
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

      auto mesh = m_resources.create<Mesh>("su37");

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

    {
      auto mesh = m_resources.create<Mesh>("land");
      Vertex vertices[]{
        Vertex{ { -1000.0f, 0.0f, -1000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { 1000.0f, 0.0f, -1000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { 1000.0f, 0.0f, 1000.0f }, { 0.0f, 1.0f, 0.0f } },
        Vertex{ { -1000.0f, 0.0f, 1000.0f }, { 0.0f, 1.0f, 0.0f } },
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
      auto mesh = m_resources.create<Mesh>("object");
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
      auto shader = m_resources.create<Shader>("default");
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
      auto shader = m_resources.create<Shader>("debug.drawline");
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
      auto shader = m_resources.create<Shader>("debug.drawscreentext");
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
      auto material = m_resources.create<Material>("default.land");
      material->initialize(m_resources.get<Shader>("default"));
      material->setValue("Material.color", glm::vec3{ 0.0f, 1.0f, 0.0f });
    }

    {
      auto material = m_resources.create<Material>("default.object");
      material->initialize(m_resources.get<Shader>("default"));
      material->setValue("Material.color", glm::vec3{ 1.0f, 1.0f, 1.0f });
    }

    {
      auto material = m_resources.create<Material>("su37.body");
      material->initialize(m_resources.get<Shader>("default"));
      material->setValue("Material.color", glm::vec3{ 0.2f, 0.400000f, 0.200000f });
    }

    {
      auto material = m_resources.create<Material>("su37.cockpit");
      material->initialize(m_resources.get<Shader>("default"));
      material->setValue("Material.color", glm::vec3{ 0.274425f, 0.282128f, 0.800000f });
    }

    {
      auto material = m_resources.create<Material>("su37.engine");
      material->initialize(m_resources.get<Shader>("default"));
      material->setValue("Material.color", glm::vec3{ 0.100000f, 0.100000f, 0.100000f });
    }

    {
      Texture2D::Options options;
      options.path = "./debug.font.png";
      auto texture = m_resources.create<Texture2D>("debug.font");
      texture->initialize(options);
    }
  }

  void setUpScene() {
    Object* movingObjectGeneratorObject = m_scene.makeObject();
    movingObjectGeneratorObject->addComponent<MovingObjectGeneratorScript>();

    Object* landObject = m_scene.makeObject();
    landObject->transform.position = { 0.0f, -10.0f, 0.0f };
    MeshRenderer* landObjectMeshRenderer = landObject->addComponent<MeshRenderer>();
    landObjectMeshRenderer->mesh = m_resources.get<Mesh>("land");
    landObjectMeshRenderer->materials.push_back(m_resources.get<Material>("default.land"));

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
      meshRenderer->mesh = m_resources.get<Mesh>("su37");
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.body"));
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.cockpit"));
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.engine"));
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
      meshRenderer->mesh = m_resources.get<Mesh>("su37");
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.body"));
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.cockpit"));
      meshRenderer->materials.push_back(m_resources.get<Material>("su37.engine"));
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

      glEnable(GL_DEPTH_TEST);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
          material->shader->vertShader.setUniform("model", meshRenderer->transform.asMatrix4());
          material->shader->vertShader.setUniform("time", float(m_time.timeSinceStart));
          material->shader->fragShader.setUniform("time", float(m_time.timeSinceStart));
          glDrawElements(GL_TRIANGLES, submesh.indexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(submesh.indexStart * sizeof(uint32_t)));
        }
      }

      glDisable(GL_DEPTH_TEST);
      {
        Shader* lineShader = m_resources.get<Shader>("debug.drawline");
        glBindVertexArray(vaoDebug);
        glBindProgramPipeline(lineShader->programPipeline);
        for (auto& command : m_debug.m_drawLineCommands) {
          lineShader->vertShader.setUniformArray("verts[0]", 2, command.verts);
          lineShader->vertShader.setUniform("color", command.color);
          glDrawArrays(GL_LINES, 0, 2);
        }
      }

      {
        Shader* screenTextShader = m_resources.get<Shader>("debug.drawscreentext");
        Texture2D* textTexture = m_resources.get<Texture2D>("debug.font");
        glBindVertexArray(vaoDebug);
        glBindProgramPipeline(screenTextShader->programPipeline);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textTexture->handle);

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
            screenTextShader->vertShader.setUniform("screenPosition", glm::vec2(x, y));
            screenTextShader->vertShader.setUniform("screenCharSize", glm::vec2(charScreenWidth, charScreenHeight));
            screenTextShader->vertShader.setUniform("charBottomLeft", glm::vec2(cx * charTextureWidth, cy * charTextureHeight));
            screenTextShader->vertShader.setUniform("charSize", glm::vec2(charTextureWidth, charTextureHeight));
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += charScreenWidth;
          }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
      }

      glBindVertexArray(0);
      glBindProgramPipeline(0);

      m_debug.clear();

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
