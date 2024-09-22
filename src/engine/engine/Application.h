#pragma once
#include <engine/Debug.h>
#include <engine/Input.h>
#include <engine/Random.h>
#include <engine/Resources.h>
#include <engine/Scene.h>
#include <engine/Time.h>

#include <SDL_video.h>
#include <glad/glad.h>

#include <memory>

class Application {
public:
  virtual ~Application();

  int setUp();
  void shutDown();
  void run();

protected:
  // related to resource import
  void importMesh(const char* filename, StringID sid);

  // todo: this one should read things from manifest of sorts
  virtual void setUpResources();

  // todo: this one should read things from scene description file
  virtual void setUpScene();

private:
  static void glDebugCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* user_param) {
    printf("gl error: %s\n", msg);
  }

  void handleEvents();

private:
  SDL_Window* m_window = nullptr;
  SDL_GLContext m_context = nullptr;
  bool m_quit = false;
  bool m_shutdownCalled = false;

protected:
  int m_width, m_height;
  Scene m_scene;
  Input m_input;
  Time m_time;
  Random m_random;
  Resources m_resources;
  Debug m_debug;
};
