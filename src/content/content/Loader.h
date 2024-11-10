#pragma once

extern "C" {
struct Planegame_ScriptLoader;

struct Planegame_ScriptLoader* Planegame_createScriptLoader();

void* Planegame_createScript(struct Planegame_ScriptLoader* scriptLoader, const char* scriptTypeName, void* object);

void Planegame_destroyScriptLoader(struct Planegame_ScriptLoader* loader);
}
