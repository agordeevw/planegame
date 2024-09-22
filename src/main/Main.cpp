#include <content/MovingObjectGeneratorScript.h>
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

class MainApplication : public Application {
public:
  // todo: this one should read things from manifest of sorts
  void setUpResources() override {
    Application::setUpResources();

    {
      auto material = m_resources.create<Material>(SID("default.land"));
      material->initialize(m_resources.get<Shader>(SID("default.land")));
      material->setValue("Material.color", glm::vec3{ 0.4f, 0.2f, 0.1f });
    }

    {
      auto material = m_resources.create<Material>(SID("default.object"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 1.0f, 1.0f, 1.0f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.body"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.2f, 0.400000f, 0.200000f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.cockpit"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.274425f, 0.282128f, 0.800000f });
    }

    {
      auto material = m_resources.create<Material>(SID("su37.engine"));
      material->initialize(m_resources.get<Shader>(SID("default")));
      material->setValue("Material.color", glm::vec3{ 0.100000f, 0.100000f, 0.100000f });
    }

    {
      auto material = m_resources.create<Material>(SID("sky"));
      material->initialize(m_resources.get<Shader>(SID("screenspace.sky")));
    }
  }

  // todo: this one should read things from scene description file
  void setUpScene() override {
    // Object* movingObjectGeneratorObject = m_scene.makeObject();
    // movingObjectGeneratorObject->addComponent<MovingObjectGeneratorScript>();

    Object* landObject = m_scene.makeObject();
    landObject->transform.position = { 0.0f, -10.0f, 0.0f };
    MeshRenderer* landObjectMeshRenderer = landObject->addComponent<MeshRenderer>();
    landObjectMeshRenderer->mesh = m_resources.get<Mesh>(SID("land"));
    landObjectMeshRenderer->materials.push_back(m_resources.get<Material>(SID("default.land")));

    Object* lights[2];
    lights[0] = m_scene.makeObject();
    lights[0]->transform.rotation = glm::quatLookAt(glm::vec3{ -1.0, -1.0, -1.0 }, glm::vec3{ 0.0, 1.0, 0.0 });
    Light* light0 = lights[0]->addComponent<Light>();
    light0->color = { 2.0f, 2.0f, 2.0f };
    light0->type = LightType::DIRECTIONAL;
    lights[1] = m_scene.makeObject();
    lights[1]->transform.position = { 2.0, 2.0f, 0.0f };
    Light* light1 = lights[1]->addComponent<Light>();
    light1->color = { 3.0, 0.0, 0.0 };
    light1->type = LightType::POINT;

    Object* plane = m_scene.makeObject();
    {
      plane->tag = 0;
      plane->transform.position = { 0.0f, 50.0f, 0.0f };
      MeshRenderer* meshRenderer = plane->addComponent<MeshRenderer>();
      meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
      meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
      plane->addComponent<PlaneControlScript>();

      Object* childLight = m_scene.makeObject();
      plane->addChild(childLight);
      Light* light = childLight->addComponent<Light>();
      childLight->transform.position = { 0.0f, 0.0f, 4.0f };
      light->color = { 5.0f, 5.0f, 5.0f };
    }

    // Object* plane1 = m_scene.makeObject();
    // {
    //   plane1->transform.position = { 0.0f, 10.0f, 0.0f };
    //   MeshRenderer* meshRenderer = plane1->addComponent<MeshRenderer>();
    //   meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
    //   meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
    //   meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
    //   meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
    // }

    Object* mainCameraObject = m_scene.makeObject();
    {
      Camera* mainCamera = mainCameraObject->addComponent<Camera>();
      mainCamera->aspectRatio = float(m_width) / float(m_height);
      mainCamera->isMain = true;
      mainCamera->fov = glm::radians(70.0f);
      mainCameraObject->addComponent<PlaneChaseCameraScript>();
    }
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
