#pragma once
#include <engine/Debug.h>
#include <engine/Input.h>
#include <engine/Random.h>
#include <engine/Resources.h>
#include <engine/Scene.h>
#include <engine/Time.h>

#include <SDL_video.h>
#include <glad/glad.h>

#include <functional>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>

class Application {
public:
  virtual ~Application();

  int setUp();
  void shutDown();
  void run();

protected:
  // related to resource import
  Mesh* importMesh(const char* filename, StringID sid);

  virtual void loadScriptLoader() = 0;
  virtual void unloadScriptLoader() = 0;
  virtual Script* createScript(const char* scriptTypeName, Object& object) = 0;

  virtual void setUpResources();

  virtual void setUpScene();

  void serializeScene(std::ostream& os);
  void deserializeScene(std::istream& is);

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
  bool m_resizeWindow = false;

protected:
  int m_width, m_height;
  Scene m_scene;
  Input m_input;
  Time m_time;
  Random m_random;
  Resources m_resources;
  Debug m_debug;
  std::unordered_map<void*, std::string> m_mapObjectToName;
};
