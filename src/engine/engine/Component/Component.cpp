#include <engine/Component/Component.h>
#include <engine/Object.h>

Component::Component(Object& object)
  : object(object), transform(object.transform) {}
