#include "Camera.h"
#include "Transform.h"

#include <SDL.h>
#include <planegame/Renderer/glad/glad.h>

#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;
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
    for (auto* child : m_children) {
      child->destroy();
    }
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
    static_assert(std::is_base_of_v<Component, T>);
    static_assert(std::is_final_v<T>);
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

struct ScriptContext {
  Scene* scene;
  const Input* input;
  const Time* time;
  Random* random;
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
    for (std::size_t i = 0; i < vertexAttributes.size(); i++) {
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
    program = glCreateShaderProgramv(options.type, 1, &options.source);
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

  GLuint program;

private:
  bool checkLinkStatus(GLuint program) {
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
      char buffer[1024];
      glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
      printf("fs: %s\n", buffer);
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

    printf("ShaderProgram\n");
    for (GLint i = 0; i < numUniformBlocks; i++) {
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

class MeshRenderer : public Component {
public:
  Mesh* mesh;
  std::vector<Material*> materials;
};

class Scene {
public:
  Object* makeObject() {
    return objects.emplace_back(std::make_unique<Object>(*this)).get();
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

  void destroyObjects() {
    for (auto& object : objects) {
      if (object->taggedDestroyed) {
        for (auto& component : object->m_components) {
          component->taggedDestroyed = true;
        }
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
    deleteTagged(scripts.activeScripts);

    deleteTagged(objects);
  }

  std::vector<std::unique_ptr<Object>> objects;
  struct Components {
    std::vector<std::unique_ptr<Camera>> cameras;
  } components;
  struct Scripts {
    std::vector<std::unique_ptr<Script>> activeScripts;
    std::vector<std::unique_ptr<Script>> pendingScripts;
  } scripts;
  ScriptContext scriptContext;
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
      generatedObjects.push_back(newObject);
      Object* newObjectParent = scene().makeObject();
      newObjectParent->addComponent<MovingObjectScript>();
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

    SDL_Quit();
  }

  GLuint attrib(GLuint idx) { return idx; }
  GLuint binding(GLuint idx) { return idx; }

  static void glDebugCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* user_param) {
    printf("gl error: %s\n", msg);
  }

  void run() {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugCallback, this);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

    m_scene.scriptContext.input = &m_input;
    m_scene.scriptContext.random = &m_random;
    m_scene.scriptContext.time = &m_time;
    m_scene.scriptContext.scene = &m_scene;

    SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
    Object* mainCameraObject = m_scene.makeObject();
    Camera* mainCamera = mainCameraObject->addComponent<Camera>();
    mainCamera->aspectRatio = float(m_width) / float(m_height);
    mainCamera->object.transform.position = glm::vec3{ 0.0, 0.0, 10.0 };
    mainCamera->isMain = true;
    mainCamera->fov = glm::radians(100.0f);
    mainCameraObject->addComponent<FPSCameraScript>();

    Object* movingObjectGeneratorObject = m_scene.makeObject();
    movingObjectGeneratorObject->addComponent<MovingObjectGeneratorScript>();

    Object* landObject = m_scene.makeObject();
    landObject->transform.position = { 0.0f, -10.0f, 0.0f };

    Mesh landMesh;
    {
      Vertex points[]{
        Vertex{ { -100.0f, 0.0f, -100.0f }, { 0.2f, 0.5f, 0.2f } },
        Vertex{ { 100.0f, 0.0f, -100.0f }, { 0.2f, 0.5f, 0.2f } },
        Vertex{ { 100.0f, 0.0f, 100.0f }, { 0.2f, 0.5f, 0.2f } },
        Vertex{ { -100.0f, 0.0f, 100.0f }, { 0.2f, 0.5f, 0.2f } },
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
      options.vertexBufferData = points;
      options.vertexCount = 4;
      landMesh.initialize(options);
    }

    Mesh objectMesh;
    {
      Vertex points[]{
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
      options.vertexBufferData = points;
      options.vertexCount = 3;
      objectMesh.initialize(options);
    }

    Shader shader;
    {
      Shader::Options options{};
      options.vertexSource = "#version 460\n"
                             "layout (location = 0) in vec3 inPosition;\n"
                             "layout (location = 1) in vec3 inColor;\n"
                             "layout (location = 2) in vec2 inUV;\n"
                             "layout (location = 0) out vec3 position;\n"
                             "layout (location = 1) out vec3 color;\n"
                             "out gl_PerVertex {\n"
                             "  vec4 gl_Position;\n"
                             "  float gl_PointSize;\n"
                             "  float gl_ClipDistance[];\n"
                             "};\n"
                             "layout (std140, binding = 2) uniform Material {\n"
                             "  vec3 color;\n"
                             "  vec3 ambient;\n"
                             "} material;\n"
                             "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                             "uniform mat4 model;"
                             "void main() {\n"
                             "  gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
                             "  position = (model * vec4(inPosition, 1.0)).xyz;\n"
                             "  color = inColor.rgb;\n"
                             "}";
      options.fragmentSource = "#version 460\n"
                               "layout (location = 0) in vec3 position;\n"
                               "layout (location = 1) in vec3 color;\n"
                               "layout (location = 0) out vec4 fragColor;\n"
                               "struct Light { vec3 position; vec3 color; };"
                               "layout (std140, binding = 1) uniform Lights {\n"
                               "  vec3 positions[128];\n"
                               "  vec3 colors[128];\n"
                               "} lights;\n"
                               "layout (std140, binding = 2) uniform Material {\n"
                               "  vec3 color;\n"
                               "  vec3 ambient;\n"
                               "} material;\n"
                               "uniform float time;"
                               "uniform int numLights;"
                               "uniform bool enableCheckerboard;"
                               "void main() {\n"
                               "  float surfColorMul = 1.0;\n"
                               "  if (enableCheckerboard && ((int(0.5 * position.x) + int(0.5 * position.z)) & 1) == 1)\n"
                               "    surfColorMul = 0.5;\n"
                               "  vec3 lightsColor = vec3(0.0,0.0,0.0);\n"
                               "  for (int i = 0; i < numLights; i++) { lightsColor += lights.colors[i] / (1.0 + length(position - lights.positions[i])); }"
                               "  fragColor = vec4(surfColorMul * material.color * (material.ambient + lightsColor), 1.0);\n"
                               "}";
      shader.initialize(options);
    }

    Material materialLand;
    materialLand.initialize(&shader);
    materialLand.setValue("Material.color", glm::vec3{ 0.0f, 1.0f, 0.0f });

    Material materialObject;
    materialObject.initialize(&shader);
    materialObject.setValue("Material.color", glm::vec3{ 1.0f, 1.0f, 1.0f });

    // render (basic):
    // update scene uniforms (camera, lights)
    // for each mesh renderer component:
    //   bind vao for mesh
    //   for each submesh in mesh:
    //     bind associated material shader
    //     update shader uniforms from material
    //     draw

    GLint ubOffsetAlignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ubOffsetAlignment);
    GLuint uniformBuffers[3];
    glCreateBuffers(3, uniformBuffers);
    glNamedBufferData(uniformBuffers[0], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[1], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[2], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
    std::vector<char> uniformBufferStorage[3];
    uniformBufferStorage[0].resize(64 * 1024);
    uniformBufferStorage[1].resize(64 * 1024);
    uniformBufferStorage[2].resize(64 * 1024);

    SDL_ShowWindow(m_window);

    SDL_bool relativeMouseMode = SDL_TRUE;
    SDL_SetRelativeMouseMode(relativeMouseMode);
    m_time.timeSinceStart = 0.0;
    uint64_t ticksLast = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    while (true) {
      handleEvents();
      if (m_quit)
        break;

      uint64_t ticksNow = SDL_GetPerformanceCounter();
      uint64_t ticksDiff = ticksNow - ticksLast;
      double delta = double(ticksDiff) / double(frequency);
      m_time.timeSinceStart += delta;
      m_time.dt = float(delta);
      ticksLast = ticksNow;

      // Update components
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

      glBindVertexArray(landMesh.vao);
      glBindProgramPipeline(shader.programPipeline);
      shader.vertShader.setUniform("time", float(m_time.timeSinceStart));
      shader.fragShader.setUniform("time", float(m_time.timeSinceStart));

      glVertexArrayVertexBuffer(landMesh.vao, 0, landMesh.vertexBuffer, 0, landMesh.vertexSize);
      glVertexArrayElementBuffer(landMesh.vao, landMesh.elementBuffer);

      auto alignedValue = [](uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & (~(alignment - 1));
      };

      // Set up scene objects
      {
        char* uniformBuffer0 = uniformBufferStorage[0].data();
        char* viewProjectionSubBuffer = uniformBuffer0;
        uint64_t viewProjectionSubBufferSize = alignedValue(shader.vertShader.getUniformBlock("ViewProjection")->size, ubOffsetAlignment);
        uint64_t viewProjectionSubBufferOffset = viewProjectionSubBuffer - uniformBuffer0;

        {
          glm::mat4 view = m_scene.getMainCamera()->viewMatrix4();
          glm::mat4 projection = m_scene.getMainCamera()->projectionMatrix4();
          memcpy(viewProjectionSubBuffer + shader.vertShader.getUniformBlock("ViewProjection")->getEntry("view")->offset, &view, sizeof(view));
          memcpy(viewProjectionSubBuffer + shader.vertShader.getUniformBlock("ViewProjection")->getEntry("projection")->offset, &projection, sizeof(projection));
        }

        char* lightsSubBuffer = viewProjectionSubBuffer + viewProjectionSubBufferSize;
        uint64_t lightsSubBufferSize = alignedValue(shader.fragShader.getUniformBlock("Lights")->size, ubOffsetAlignment);
        uint64_t lightsSubBufferOffset = lightsSubBuffer - uniformBuffer0;

        {
          const int numLights = 2;
          glm::vec3 positions[numLights]{
            { 0.0f, 10.0f * std::sin(m_time.timeSinceStart), 0.0f },
            { 10.0f * std::sin(m_time.timeSinceStart), 10.0f * std::sin(m_time.timeSinceStart), 0.0f }
          };
          glm::vec3 colors[numLights]{
            { 5.0f, 5.0f, 5.0f },
            { 3.0f, 0.0f, 0.0f }
          };

          for (int i = 0; i < numLights; i++) {
            auto positionEntry = shader.fragShader.getUniformBlock("Lights")->getEntry("Lights.positions[0]");
            auto colorEntry = shader.fragShader.getUniformBlock("Lights")->getEntry("Lights.colors[0]");
            memcpy(lightsSubBuffer + positionEntry->offset + i * positionEntry->stride, &positions[i], sizeof(positions[i]));
            memcpy(lightsSubBuffer + colorEntry->offset + i * colorEntry->stride, &colors[i], sizeof(colors[i]));
          }
          shader.fragShader.setUniform("numLights", numLights);
        }

        char* uniformBuffer0End = lightsSubBuffer + lightsSubBufferSize;

        uint64_t uniformBuffer0Size = uniformBuffer0End - uniformBuffer0;

        glNamedBufferSubData(uniformBuffers[0], 0, uniformBuffer0Size, uniformBuffer0);

        glBindBufferRange(GL_UNIFORM_BUFFER, shader.vertShader.getUniformBlock("ViewProjection")->binding, uniformBuffers[0], viewProjectionSubBufferOffset, viewProjectionSubBufferSize);
        glBindBufferRange(GL_UNIFORM_BUFFER, shader.fragShader.getUniformBlock("Lights")->binding, uniformBuffers[0], lightsSubBufferOffset, lightsSubBufferSize);
      }

      shader.vertShader.setUniform("model", landObject->transform.asMatrix4());
      char* uniformBuffer2 = uniformBufferStorage[2].data();
      char* materialSubBuffer = uniformBuffer2;
      uint32_t materialSubBufferSize = alignedValue(shader.fragShader.getUniformBlock("Material")->size, ubOffsetAlignment);

      {
        glNamedBufferSubData(uniformBuffers[2], 0, materialLand.materialUniformBlock->size, materialLand.uniformStorage.get());
        glBindBufferRange(GL_UNIFORM_BUFFER, materialLand.materialUniformBlock->binding, uniformBuffers[2], 0, materialLand.materialUniformBlock->size);
        shader.fragShader.setUniform("enableCheckerboard", true);
      }
      glDrawElements(GL_TRIANGLES, landMesh.indexCount, GL_UNSIGNED_INT, nullptr);

      glBindVertexArray(objectMesh.vao);
      glVertexArrayVertexBuffer(objectMesh.vao, 0, objectMesh.vertexBuffer, 0, objectMesh.vertexSize);
      glVertexArrayElementBuffer(objectMesh.vao, objectMesh.elementBuffer);

      for (auto& script : m_scene.scripts.activeScripts) {
        if (dynamic_cast<MovingObjectScript*>(script.get())) {
          shader.vertShader.setUniform("model", script->object.transform.asMatrix4());
          {
            shader.fragShader.setUniform("enableCheckerboard", false);
            glNamedBufferSubData(uniformBuffers[2], 0, materialObject.materialUniformBlock->size, materialObject.uniformStorage.get());
            glBindBufferRange(GL_UNIFORM_BUFFER, materialObject.materialUniformBlock->binding, uniformBuffers[2], 0, materialObject.materialUniformBlock->size);
          }
          glDrawElements(GL_TRIANGLES, objectMesh.indexCount, GL_UNSIGNED_INT, nullptr);
        }
      }

      SDL_GL_SwapWindow(m_window);
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
};

int main(int, char**) {
  Application app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
