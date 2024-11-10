#include <content/PlaneChaseCameraScript.h>
#include <content/PlaneControlScript.h>
#include <engine/Application.h>
#include <engine/Component/Camera.h>
#include <engine/Component/Light.h>
#include <engine/Component/MeshRenderer.h>
#include <engine/Object.h>
#include <engine/Renderer/Material.h>
#include <engine/Resources.h>
#include <engine/Scene.h>

#include <SDL_main.h>

#include <typeinfo>

class MainApplication : public Application {
public:
  void setUpResources() override {
    Application::setUpResources();

    registerScript<PlaneControlScript>();
    registerScript<PlaneChaseCameraScript>();
  }
};

int main(int, char**) {
  MainApplication app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
