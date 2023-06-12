#pragma once
#include <planegame/Debug.h>
#include <planegame/Input.h>
#include <planegame/Random.h>
#include <planegame/Resources.h>
#include <planegame/Scene.h>
#include <planegame/Time.h>

#include <SDL_video.h>
#include <glad/glad.h>

#include <memory>

class Application {
public:
  ~Application();

  int setUp();

  void shutDown();

  static void glDebugCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* user_param) {
    printf("gl error: %s\n", msg);
  }

  // related to resource import
  void importMesh(const char* filename, StringID sid);

  // todo: this one should read things from manifest of sorts
  void setUpResources();

  // todo: this one should read things from scene description file
  void setUpScene();

  void run();

private:
  void handleEvents();

  SDL_Window* m_window = nullptr;
  SDL_GLContext m_context = nullptr;
  bool m_quit = false;
  bool m_shutdownCalled = false;
  int m_width, m_height;
  Scene m_scene;
  Input m_input;
  Time m_time;
  Random m_random;
  Resources m_resources;
  Debug m_debug;
};
