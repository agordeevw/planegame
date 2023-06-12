#include <planegame/Component/Component.h>
#include <planegame/Object.h>

Component::Component(Object& object)
  : object(object), transform(object.transform) {}
