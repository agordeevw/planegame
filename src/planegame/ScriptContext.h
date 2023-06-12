#pragma once

class Scene;
class Resources;
class Debug;
class Input;
class Time;
class Random;

struct ScriptContext {
  Scene* scene;
  const Input* input;
  const Time* time;
  Random* random;
  Resources* resources;
  Debug* debug;
};
