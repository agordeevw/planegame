#include <planegame/Component/Script.h>
#include <planegame/Component/Transform.h>

Script::Script(Object& object)
  : Component(object) {
  SCRIPT_REGISTER_PROPERTY(transform.position);
}

void Script::registerProperty(NamedProperty property) {
  m_properties.push_back(property);
  m_mapNameIDToPropertyIndex[makeSID(property.name)] = m_properties.size() - 1;
}
