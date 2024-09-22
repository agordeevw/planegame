#include "MissileScript.h"
#include <content/PlaneControlScript.h>
#include <engine/ScriptIncludes.h>

MissileScript::MissileScript(Object& object) : Script(object) {
}

void MissileScript::initialize() {
  velocity = initialVelocity;
  timeToDie = initialTime;
}

void MissileScript::update() {
  transform.position += velocity * time().dt;
  timeToDie -= time().dt;
  if (timeToDie <= 0.0f) {
    object.destroy();
  }
}
