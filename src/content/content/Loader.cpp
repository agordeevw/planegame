#include <content/Loader.h>

#include <content/MissileScript.h>
#include <content/PlaneChaseCameraScript.h>
#include <content/PlaneControlScript.h>

#include <engine/StringID.h>

#include <string>
#include <unordered_map>

extern "C" {
struct Planegame_ScriptLoader {
  std::unordered_map<StringID, Script* (*)(Object&), StringIDHasher> m_mapScriptTypeNameStringIdToCreateFunction;
};

struct Planegame_ScriptLoader* Planegame_createScriptLoader() {
  struct Planegame_ScriptLoader* ret = new struct Planegame_ScriptLoader;
  ret->m_mapScriptTypeNameStringIdToCreateFunction.emplace(makeSID(MissileScript::name()), [](Object& object) -> Script* { return new MissileScript(object); });
  ret->m_mapScriptTypeNameStringIdToCreateFunction.emplace(makeSID(PlaneChaseCameraScript::name()), [](Object& object) -> Script* { return new PlaneChaseCameraScript(object); });
  ret->m_mapScriptTypeNameStringIdToCreateFunction.emplace(makeSID(PlaneControlScript::name()), [](Object& object) -> Script* { return new PlaneControlScript(object); });
  return ret;
}

void* Planegame_createScript(struct Planegame_ScriptLoader* scriptLoader, const char* scriptTypeName, void* object) {
  StringID sid = makeSID(scriptTypeName);
  return scriptLoader->m_mapScriptTypeNameStringIdToCreateFunction.at(sid)(*(Object*)(object));
}

void Planegame_destroyScriptLoader(struct Planegame_ScriptLoader* loader) {
  delete loader;
}
}
