#include <content/Loader.h>
#include <engine/Application.h>

#include <SDL_main.h>

#include <Windows.h>

class MainApplication : public Application {
public:
  void loadScriptLoader() override {
    CopyFile("planegame.content.dll", "planegame.content.dll.temp", FALSE);

    m_contentLibrary = LoadLibraryA("planegame.content.dll.temp");
    Planegame_createScriptLoader = (decltype(Planegame_createScriptLoader))GetProcAddress(m_contentLibrary, "Planegame_createScriptLoader");
    Planegame_createScript = (decltype(Planegame_createScript))GetProcAddress(m_contentLibrary, "Planegame_createScript");
    Planegame_destroyScriptLoader = (decltype(Planegame_destroyScriptLoader))GetProcAddress(m_contentLibrary, "Planegame_destroyScriptLoader");

    m_scriptLoader = Planegame_createScriptLoader();
  }
  void unloadScriptLoader() override {
    Planegame_destroyScriptLoader(m_scriptLoader);
    m_scriptLoader = nullptr;
    FreeLibrary(m_contentLibrary);
    m_contentLibrary = 0;
  }
  Script* createScript(const char* scriptTypeName, Object& object) override {
    return (Script*)Planegame_createScript(m_scriptLoader, scriptTypeName, &object);
  }

private:
  HMODULE m_contentLibrary = {};

  struct Planegame_ScriptLoader* (*Planegame_createScriptLoader)();
  void* (*Planegame_createScript)(struct Planegame_ScriptLoader* scriptLoader, const char* scriptTypeName, void* object);
  void (*Planegame_destroyScriptLoader)(struct Planegame_ScriptLoader* loader);

  struct Planegame_ScriptLoader* m_scriptLoader;
};

int main(int, char**) {
  MainApplication app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  DeleteFile("planegame.content.dll.temp");

  return 0;
}
