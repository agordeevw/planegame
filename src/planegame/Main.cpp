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
  glm::vec2 uv;
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
    enum class Type {
      Unknown,
      Position,
      Normal,
      Tangent,
      Color,
      UV,
    };

    enum class Format {
      Unknown,
      f16,
      f32,
    };

    Type type = Type::Unknown;
    Format format = Format::Unknown;
    int dimension = -1;
    int binding = 0;
  };

  struct SubMesh {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    // bounds
  };

  enum class IndexFormat {
    Unknown,
    u16,
    u32,
  };

  std::vector<SubMesh> submeshes;
  std::vector<VertexAttribute> vertexAttributes;
  uint32_t vertexCount = 0;
  IndexFormat indexFormat = IndexFormat::Unknown;
  uint32_t indexCount = 0;
};

struct Shader {
  struct Options {
    const char* vertexSource = nullptr;
    const char* fragmentSource = nullptr;
  };

  void initialize(const Options& options) {
    programV = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &options.vertexSource);
    checkLinkStatus(programV);

    programF = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &options.fragmentSource);
    checkLinkStatus(programF);

    glCreateProgramPipelines(1, &programPipeline);
    glUseProgramStages(programPipeline, GL_VERTEX_SHADER_BIT, programV);
    glUseProgramStages(programPipeline, GL_FRAGMENT_SHADER_BIT, programF);

    listProgramUniforms(programV);
    listProgramUniforms(programF);
  }

  GLuint programV = -1;
  GLuint programF = -1;
  GLuint programPipeline = -1;

private:
  void checkLinkStatus(GLuint program) {
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
      char buffer[1024];
      glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
      printf("fs: %s\n", buffer);
    }
  }

  void listProgramUniforms(GLuint program) {
    char buffer[1024];
    GLint uniformCount = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
    if (uniformCount != 0) {
      GLint maxNameLength;
      glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
      auto uniformName = std::make_unique<char[]>(maxNameLength);
      for (GLint i = 0; i < uniformCount; i++) {
        GLint size;
        GLenum type;
        glGetActiveUniform(program, i, sizeof(buffer), nullptr, &size, &type, buffer);
        printf("%s: size(%d) type(%d)\n", buffer, size, type);
      }
    }
  }
};

struct Material {
  Shader* shader;
  // editing values for uniforms here
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

    GLuint vertexBufferLand;
    glCreateBuffers(1, &vertexBufferLand);
    Vertex landPoints[]{
      Vertex{ { -100.0f, 0.0f, -100.0f }, { 0.2f, 0.5f, 0.2f } },
      Vertex{ { 100.0f, 0.0f, -100.0f }, { 0.2f, 0.5f, 0.2f } },
      Vertex{ { 100.0f, 0.0f, 100.0f }, { 0.2f, 0.5f, 0.2f } },
      Vertex{ { -100.0f, 0.0f, 100.0f }, { 0.2f, 0.5f, 0.2f } },
    };
    glNamedBufferData(vertexBufferLand, sizeof(landPoints), landPoints, GL_STATIC_DRAW);

    GLuint elementBufferLand;
    glCreateBuffers(1, &elementBufferLand);
    uint32_t indicesLand[]{ 0, 2, 1, 0, 3, 2 };
    glNamedBufferData(elementBufferLand, sizeof(indicesLand), indicesLand, GL_STATIC_DRAW);

    GLuint vertexBuffer;
    glCreateBuffers(1, &vertexBuffer);
    Vertex points[]{
      Vertex{ { -1.0, -1.0, 0.0 }, { 1.0, 0.0, 0.0 } },
      Vertex{ { 1.0, -1.0, 0.0 }, { 0.0, 1.0, 0.0 } },
      Vertex{ { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } }
    };
    glNamedBufferData(vertexBuffer, sizeof(points), points, GL_STATIC_DRAW);

    GLuint elementBuffer;
    glCreateBuffers(1, &elementBuffer);
    uint32_t indices[]{ 0, 1, 2 };
    glNamedBufferData(elementBuffer, sizeof(indices), indices, GL_STATIC_DRAW);

    struct ViewProjectionMatrix {
      glm::mat4x4 viewMatrix4;
      glm::mat4x4 projectionMatrix4;
    } cameraViewProjection;

    struct TransformMatrices {
      glm::mat4x4 transforms[128];
    } objectTransforms;

    GLuint uniformBuffers[3];
    glCreateBuffers(3, uniformBuffers);
    glNamedBufferData(uniformBuffers[0], sizeof(ViewProjectionMatrix), nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[1], sizeof(TransformMatrices), nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[2], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);

    GLuint vertexArray;
    glCreateVertexArrays(1, &vertexArray);
    {
      glEnableVertexArrayAttrib(vertexArray, attrib(0));
      glVertexArrayAttribFormat(vertexArray, attrib(0), 3, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribBinding(vertexArray, attrib(0), binding(0));
      glEnableVertexArrayAttrib(vertexArray, attrib(1));
      glVertexArrayAttribFormat(vertexArray, attrib(1), 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
      glVertexArrayAttribBinding(vertexArray, attrib(1), binding(0));
      glEnableVertexArrayAttrib(vertexArray, attrib(2));
      glVertexArrayAttribFormat(vertexArray, attrib(2), 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
      glVertexArrayAttribBinding(vertexArray, attrib(2), binding(0));
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
                             "layout (binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                             "layout (binding = 1) uniform Transform { mat4 transforms[128]; };\n"
                             "void main() {\n"
                             "  gl_Position = projection * view * transforms[gl_InstanceID] * vec4(inPosition, 1.0);\n"
                             "  position = (transforms[gl_InstanceID] * vec4(inPosition, 1.0)).xyz;\n"
                             "  color = inColor.rgb;\n"
                             "}";
      options.fragmentSource = "#version 460\n"
                               "layout (location = 0) in vec3 position;\n"
                               "layout (location = 1) in vec3 color;\n"
                               "out vec4 fragColor;\n"
                               "layout (binding = 2) uniform Material {\n"
                               "  vec3 color;"
                               "} material;\n"
                               "void main() {\n"
                               "  if (((int(0.5 * position.x) + int(0.5 * position.z)) & 1) == 1)\n"
                               "    fragColor = vec4(material.color * color, 1.0);\n"
                               "  else\n"
                               "    fragColor = vec4(material.color * 0.5 * color, 1.0);\n"
                               "}";
      shader.initialize(options);
    }

    GLuint programUniformBlockIndex1 = glGetUniformBlockIndex(shader.programV, "ViewProjection");
    glUniformBlockBinding(shader.programV, programUniformBlockIndex1, 0);
    GLuint programUniformBlockIndex2 = glGetUniformBlockIndex(shader.programV, "Transform");
    glUniformBlockBinding(shader.programV, programUniformBlockIndex2, 1);
    GLuint programUniformBlockIndex3 = glGetUniformBlockIndex(shader.programF, "Material");
    glUniformBlockBinding(shader.programF, programUniformBlockIndex3, 2);

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

      // For each material:
      //   for each submesh with this material:
      //     bind submesh vertex buffers and element buffer
      //     draw elements (instanced)

      glEnable(GL_DEPTH_TEST);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glBindVertexArray(vertexArray);
      glBindProgramPipeline(shader.programPipeline);

      glVertexArrayVertexBuffer(vertexArray, binding(0), vertexBufferLand, 0, 8 * sizeof(float));
      glVertexArrayElementBuffer(vertexArray, elementBufferLand);

      {
        cameraViewProjection.viewMatrix4 = m_scene.getMainCamera()->viewMatrix4();
        cameraViewProjection.projectionMatrix4 = m_scene.getMainCamera()->projectionMatrix4();
        glNamedBufferSubData(uniformBuffers[0], 0, sizeof(cameraViewProjection), &cameraViewProjection);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, uniformBuffers[0], 0, sizeof(cameraViewProjection));
      }

      {
        float color[3]{ 1.0f, 0.5f, 0.5f };
        glNamedBufferSubData(uniformBuffers[2], 0, sizeof(color), &color);
        glBindBufferRange(GL_UNIFORM_BUFFER, 2, uniformBuffers[2], 0, sizeof(color));
      }

      {
        objectTransforms.transforms[0] = landObject->transform.asMatrix4();
        glNamedBufferSubData(uniformBuffers[1], 0, sizeof(glm::mat4x4), &objectTransforms);
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, uniformBuffers[1], 0, sizeof(glm::mat4x4));
      }

      glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, 1);

      glVertexArrayVertexBuffer(vertexArray, binding(0), vertexBuffer, 0, 8 * sizeof(float));
      glVertexArrayElementBuffer(vertexArray, elementBuffer);

      std::vector<Object*> movingObjects;
      for (auto& script : m_scene.scripts.activeScripts) {
        if (dynamic_cast<MovingObjectScript*>(script.get()) && movingObjects.size() < 100) {
          movingObjects.push_back(&script->object);
        }
      }
      {
        for (size_t i = 0; i < movingObjects.size(); i++) {
          objectTransforms.transforms[i] = movingObjects[i]->transform.asMatrix4();
        }
        glNamedBufferSubData(uniformBuffers[1], 0, sizeof(glm::mat4x4) * movingObjects.size(), &objectTransforms);
      }
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffers[1]);
      glDrawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr, int(movingObjects.size()));

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
