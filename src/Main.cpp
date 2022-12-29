#include <SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <memory>
#include <vector>

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;
  glm::vec2 uv;
};

struct Transform {
  void translate(const glm::vec3& v) {
    position += v;
  }

  void rotateLocal(const glm::vec3& v, float angle) {
    rotation = glm::rotate(rotation, angle, v);
  }

  void rotateGlobal(const glm::vec3& v, float angle) {
    rotation = glm::rotate(rotation, angle, glm::rotate(glm::inverse(rotation), v));
  }

  glm::vec3 forward() const {
    glm::vec3 ret{ 0.0, 0.0, -1.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::vec3 up() const {
    glm::vec3 ret{ 0.0, 1.0, 0.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::vec3 right() const {
    glm::vec3 ret{ 1.0, 0.0, 0.0 };
    ret = rotation * ret;
    return ret;
  }

  glm::mat4x4 transform() const {
    return glm::translate(glm::identity<glm::mat4>(), position) * glm::toMat4(rotation);
  }

  glm::vec3 position = glm::zero<glm::vec3>();
  glm::quat rotation = glm::identity<glm::quat>();
};

struct Camera {
  glm::mat4x4 view() const {
    return glm::lookAt(transform.position, transform.position + transform.forward(), transform.up());
  }

  glm::mat4x4 projection() const {
    return glm::perspective(fov, aspectRatio, 0.01f, 1000.0f);
  }

  Transform transform{};
  float fov = glm::radians(90.0f);
  float aspectRatio = 1.0f;
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

    SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);

    m_camera.aspectRatio = float(m_width) / float(m_height);
    m_camera.transform.position = glm::vec3{ 0.0, 0.0, 10.0 };

    Transform land;
    land.position.y -= 10.0;
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

    Transform objects[3]{};
    objects[1].position.x -= 10.0;
    objects[2].position.x += 10.0;

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
      glm::mat4x4 view;
      glm::mat4x4 projection;
    } cameraViewProjection;

    struct TransformMatrices {
      glm::mat4x4 transforms[128];
    } objectTransforms;

    GLuint uniformBuffers[2];
    glCreateBuffers(2, uniformBuffers);
    glNamedBufferData(uniformBuffers[0], sizeof(ViewProjectionMatrix), nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[1], sizeof(TransformMatrices), nullptr, GL_DYNAMIC_DRAW);

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

    char buffer[1024];

    GLuint shaderV = glCreateShader(GL_VERTEX_SHADER);
    {
      const char* source = "#version 460\n"
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
                           "layout(binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                           "layout(binding = 1) uniform Transform { mat4 transforms[128]; };\n"
                           "void main() {\n"
                           "  gl_Position = projection * view * transforms[gl_InstanceID] * vec4(inPosition, 1.0);\n"
                           "  position = (transforms[gl_InstanceID] * vec4(inPosition, 1.0)).xyz;\n"
                           "  color = inColor.rgb;\n"
                           "}";

      glShaderSource(shaderV, 1, &source, nullptr);
      glCompileShader(shaderV);
      glGetShaderInfoLog(shaderV, sizeof(buffer), nullptr, buffer);
      if (buffer[0])
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "vertex shader compile error", buffer, m_window);
    }

    GLuint shaderF = glCreateShader(GL_FRAGMENT_SHADER);
    {
      const char* source = "#version 460\n"
                           "layout (location = 0) in vec3 position;\n"
                           "layout (location = 1) in vec3 color;\n"
                           "out vec4 fragColor;\n"
                           "void main() {\n"
                           "  if (((int(0.5 * position.x) + int(0.5 * position.z)) & 1) == 1)\n"
                           "    fragColor = vec4(color, 1.0);\n"
                           "  else\n"
                           "    fragColor = vec4(0.5 * color, 1.0);\n"
                           "}";

      glShaderSource(shaderF, 1, &source, nullptr);
      glCompileShader(shaderF);
      glGetShaderInfoLog(shaderF, sizeof(buffer), nullptr, buffer);
      if (buffer[0])
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "fragment shader compile error", buffer, m_window);
    }

    GLuint program = glCreateProgram();
    glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glAttachShader(program, shaderV);
    glAttachShader(program, shaderF);
    glLinkProgram(program);
    glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
    if (buffer[0])
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "shader link error", buffer, m_window);

    GLuint programUniformBlockIndex1 = glGetUniformBlockIndex(program, "ViewProjection");
    glUniformBlockBinding(program, programUniformBlockIndex1, 0);
    GLuint programUniformBlockIndex2 = glGetUniformBlockIndex(program, "Transform");
    glUniformBlockBinding(program, programUniformBlockIndex2, 1);

    GLuint pipeline;
    glCreateProgramPipelines(1, &pipeline);

    glUseProgramStages(pipeline, GL_ALL_SHADER_BITS, program);

    SDL_ShowWindow(m_window);

    SDL_bool relativeMouseMode = SDL_TRUE;
    SDL_SetRelativeMouseMode(relativeMouseMode);
    double time = 0.0;
    uint64_t ticksLast = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    while (true) {
      handleEvents();
      if (m_quit)
        break;

      uint64_t ticksNow = SDL_GetPerformanceCounter();
      uint64_t ticksDiff = ticksNow - ticksLast;
      double delta = double(ticksDiff) / double(frequency);
      time += delta;
      float dt = float(delta);
      ticksLast = ticksNow;

      // Update
      {
        if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyPressed[SDL_SCANCODE_C]) {
          relativeMouseMode = relativeMouseMode == SDL_TRUE ? SDL_FALSE : SDL_TRUE;
          SDL_SetRelativeMouseMode(relativeMouseMode);
        }

        float speed = 10.0;
        if (m_input.keyDown[SDL_SCANCODE_LSHIFT]) {
          speed = 20.0;
        }

        if (m_input.keyDown[SDL_SCANCODE_W]) {
          m_camera.transform.position += m_camera.transform.forward() * speed * float(dt);
        }
        if (m_input.keyDown[SDL_SCANCODE_S]) {
          m_camera.transform.position -= m_camera.transform.forward() * speed * float(dt);
        }
        if (m_input.keyDown[SDL_SCANCODE_D]) {
          m_camera.transform.position += m_camera.transform.right() * speed * float(dt);
        }
        if (m_input.keyDown[SDL_SCANCODE_A]) {
          m_camera.transform.position -= m_camera.transform.right() * speed * float(dt);
        }
        if (m_input.keyDown[SDL_SCANCODE_Q]) {
          m_camera.transform.position += m_camera.transform.up() * speed * float(dt);
        }
        if (m_input.keyDown[SDL_SCANCODE_Z]) {
          m_camera.transform.position -= m_camera.transform.up() * speed * float(dt);
        }
        m_camera.transform.rotateGlobal(glm::vec3(0.0f, 1.0f, 0.0f), -float(dt) * m_input.mousedx);
        m_camera.transform.rotateLocal(glm::vec3(1.0f, 0.0f, 0.0f), -float(dt) * m_input.mousedy);

        objects[0].rotateLocal(objects[0].forward(), dt);
        objects[1].rotateLocal(objects[1].forward(), 0.5f * dt);
        objects[1].rotateLocal(objects[1].up(), 0.5f * dt);
        objects[2].rotateLocal(objects[2].forward(), -0.5f * dt);
        objects[2].rotateLocal(objects[2].up(), -0.5f * dt);
      }

      glEnable(GL_DEPTH_TEST);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glBindVertexArray(vertexArray);
      glBindProgramPipeline(pipeline);

      glVertexArrayVertexBuffer(vertexArray, binding(0), vertexBufferLand, 0, 8 * sizeof(float));
      glVertexArrayElementBuffer(vertexArray, elementBufferLand);

      {
        cameraViewProjection.view = m_camera.view();
        cameraViewProjection.projection = m_camera.projection();
        {
          char* buf = (char*)glMapNamedBuffer(uniformBuffers[0], GL_WRITE_ONLY);
          memcpy(buf, &cameraViewProjection, sizeof(cameraViewProjection));
          glUnmapNamedBuffer(uniformBuffers[0]);
        }
      }
      glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffers[0]);

      {
        objectTransforms.transforms[0] = land.transform();
        {
          char* buf = (char*)glMapNamedBuffer(uniformBuffers[1], GL_WRITE_ONLY);
          memcpy(buf, &objectTransforms, sizeof(glm::mat4x4) * 1);
          glUnmapNamedBuffer(uniformBuffers[1]);
        }
      }
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffers[1]);
      glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, 1);

      glVertexArrayVertexBuffer(vertexArray, binding(0), vertexBuffer, 0, 8 * sizeof(float));
      glVertexArrayElementBuffer(vertexArray, elementBuffer);

      {
        objectTransforms.transforms[0] = objects[0].transform();
        objectTransforms.transforms[1] = objects[1].transform();
        objectTransforms.transforms[2] = objects[2].transform();
        {
          char* buf = (char*)glMapNamedBuffer(uniformBuffers[1], GL_WRITE_ONLY);
          memcpy(buf, &objectTransforms, sizeof(glm::mat4x4) * 3);
          glUnmapNamedBuffer(uniformBuffers[1]);
        }
      }
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffers[1]);
      glDrawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr, 3);

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
  Camera m_camera;
  struct Input {
    bool keyDown[SDL_NUM_SCANCODES]{};
    bool prevKeyDown[SDL_NUM_SCANCODES]{};
    bool keyPressed[SDL_NUM_SCANCODES]{};
    bool keyReleased[SDL_NUM_SCANCODES]{};
    float mousedx = 0.0;
    float mousedy = 0.0;
  } m_input;
};

int main(int, char**) {
  Application app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
