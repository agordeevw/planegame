// #pragma once
// #include <engine/Component/Script.h>
// #include <engine/ScriptIncludes.h>
//
// class __declspec(dllexport) FPSCameraScript final : public Script {
// public:
//   using Script::Script;
//
//   void update() override {
//     Transform& transform = object.transform;
//
//     float speed = m_speed;
//     if (input().keyDown[SDL_SCANCODE_LSHIFT]) {
//       speed *= 2.0;
//     }
//
//     if (input().keyDown[SDL_SCANCODE_W]) {
//       transform.position += transform.forward() * speed * time().dt;
//     }
//     if (input().keyDown[SDL_SCANCODE_S]) {
//       transform.position -= transform.forward() * speed * time().dt;
//     }
//     if (input().keyDown[SDL_SCANCODE_D]) {
//       transform.position += transform.right() * speed * time().dt;
//     }
//     if (input().keyDown[SDL_SCANCODE_A]) {
//       transform.position -= transform.right() * speed * time().dt;
//     }
//     if (input().keyDown[SDL_SCANCODE_Q]) {
//       transform.position += transform.up() * speed * time().dt;
//     }
//     if (input().keyDown[SDL_SCANCODE_Z]) {
//       transform.position -= transform.up() * speed * time().dt;
//     }
//     transform.rotateGlobal(glm::vec3(0.0f, 1.0f, 0.0f), -time().dt * input().mousedx);
//     transform.rotateLocal(glm::vec3(1.0f, 0.0f, 0.0f), -time().dt * input().mousedy);
//   }
//
//   float m_speed = 10.0;
// };
