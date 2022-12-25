#include <SDL.h>
#include <glad/glad.h>

#include <memory>

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
      SDL_WINDOW_OPENGL);
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

  void run() {
    SDL_ShowWindow(m_window);

    double time = 0.0;

    GLuint vertexBuffer;
    glCreateBuffers(1, &vertexBuffer);
    float points[]{
      -1.0, -1.0, 1.0, 0.0, 0.0,
      1.0, -1.0, 0.0, 1.0, 0.0,
      0.0, 1.0, 0.0, 0.0, 1.0
    };
    glNamedBufferData(vertexBuffer, sizeof(points), points, GL_STATIC_DRAW);

    GLuint elementBuffer;
    glCreateBuffers(1, &elementBuffer);
    uint32_t indices[]{ 0, 1, 2 };
    glNamedBufferData(elementBuffer, sizeof(indices), indices, GL_STATIC_DRAW);

    GLuint uniformBuffer;
    glCreateBuffers(1, &uniformBuffer);
    float transform[4]{};
    glNamedBufferData(uniformBuffer, sizeof(transform), nullptr, GL_DYNAMIC_DRAW);

    GLuint vertexArray;
    glCreateVertexArrays(1, &vertexArray);
    {
      glEnableVertexArrayAttrib(vertexArray, attrib(0));
      glVertexArrayAttribFormat(vertexArray, attrib(0), 2, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribBinding(vertexArray, attrib(0), binding(0));
      glEnableVertexArrayAttrib(vertexArray, attrib(1));
      glVertexArrayAttribFormat(vertexArray, attrib(1), 3, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
      glVertexArrayAttribBinding(vertexArray, attrib(1), binding(0));
      glVertexArrayVertexBuffer(vertexArray, binding(0), vertexBuffer, 0, 5 * sizeof(float));
      glVertexArrayElementBuffer(vertexArray, elementBuffer);
    }

    char buffer[1024];

    GLuint shaderV = glCreateShader(GL_VERTEX_SHADER);
    {
      const char* source = "#version 460\n"
                           "layout (location = 0) in vec2 inPosition;\n"
                           "layout (location = 1) in vec3 inColor;\n"
                           "out vec2 position;\n"
                           "out vec3 color;\n"
                           "out gl_PerVertex {\n"
                           "  vec4 gl_Position;\n"
                           "  float gl_PointSize;\n"
                           "  float gl_ClipDistance[];\n"
                           "};\n"
                           "layout(row_major, binding=0) uniform Transform { mat2 transform; };\n"
                           "void main() {\n"
                           "  position = transform * inPosition.xy;\n"
                           "  color = inColor.rgb;\n"
                           "  gl_Position = vec4(position, 0.0, 1.0);\n"
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
                           "in vec2 position;\n"
                           "in vec3 color;\n"
                           "out vec4 fragColor;\n"
                           "void main() {\n"
                           "fragColor = vec4(color, 1.0);\n"
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

    GLuint programUniformBlockIndex = glGetUniformBlockIndex(program, "Transform");
    glUniformBlockBinding(program, programUniformBlockIndex, 0);

    GLuint pipeline;
    glCreateProgramPipelines(1, &pipeline);

    glUseProgramStages(pipeline, GL_ALL_SHADER_BITS, program);

    uint64_t ticksLast = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    while (true) {
      handleEvents();
      if (m_quit)
        break;

      uint64_t ticksNow = SDL_GetPerformanceCounter();
      uint64_t ticksDiff = ticksNow - ticksLast;
      time += double(ticksDiff) / double(frequency);
      ticksLast = ticksNow;

      {
        float c = std::cos(time);
        float s = std::sin(time);
        transform[0] = c;
        transform[1] = -s;
        transform[2] = s;
        transform[3] = c;

        char* buf = (char*)glMapNamedBuffer(uniformBuffer, GL_WRITE_ONLY);
        memcpy(buf, transform, sizeof(transform));
        glUnmapNamedBuffer(uniformBuffer);
      }

      glClear(GL_COLOR_BUFFER_BIT);

      glBindVertexArray(vertexArray);
      glBindProgramPipeline(pipeline);
      glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

      SDL_GL_SwapWindow(m_window);
    }
  }

private:
  void handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          m_quit = true;
          return;
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
};

int main(int, char**) {
  Application app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
