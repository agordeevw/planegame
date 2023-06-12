#include <planegame/Scripts/MovingObjectGeneratorScript.h>
#include <planegame/Scripts/MovingObjectScript.h>
#include <planegame/Scripts/ScriptIncludes.h>

void MovingObjectGeneratorScript::update() {
  if (input().keyPressed[SDL_SCANCODE_SPACE]) {
    Object* newObject = scene().makeObject();
    newObject->addComponent<MovingObjectScript>();
    {
      MeshRenderer* meshRenderer = newObject->addComponent<MeshRenderer>();
      meshRenderer->mesh = resources().get<Mesh>(SID("object"));
      meshRenderer->materials = { resources().get<Material>(SID("default.object")) };
    }
    generatedObjects.push_back(newObject);
    Object* newObjectParent = scene().makeObject();
    newObjectParent->addComponent<MovingObjectScript>();
    {
      MeshRenderer* meshRenderer = newObjectParent->addComponent<MeshRenderer>();
      meshRenderer->mesh = resources().get<Mesh>(SID("object"));
      meshRenderer->materials = { resources().get<Material>(SID("default.object")) };
    }
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
