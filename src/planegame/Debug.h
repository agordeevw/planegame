#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

class Debug {
public:
  struct DrawLineCommand {
    glm::vec3 verts[2];
    glm::vec3 color;
  };

  struct DrawScreenLineCommand {
    glm::vec2 verts[2];
    glm::vec3 color;
  };

  struct DrawScreenTextCommand {
    glm::vec2 topleft;
    std::string str;
  };

  void drawLine(glm::vec3 start, glm::vec3 end, glm::vec3 color) {
    DrawLineCommand command;
    command.verts[0] = start;
    command.verts[1] = end;
    command.color = color;
    m_drawLineCommands.push_back(command);
  }

  void drawScreenLine(glm::vec2 start, glm::vec2 end, glm::vec3 color) {
    DrawScreenLineCommand command;
    command.verts[0] = start;
    command.verts[1] = end;
    command.color = color;
    m_drawScreenLineCommands.push_back(command);
  }

  void drawScreenText(glm::vec2 topleft, const char* str) {
    DrawScreenTextCommand command;
    command.topleft = topleft;
    command.str = str;
    m_drawScreenTextCommands.push_back(command);
  }

  void clear() {
    m_drawLineCommands.clear();
    m_drawScreenLineCommands.clear();
    m_drawScreenTextCommands.clear();
  }

private:
  friend class Application;

  std::vector<DrawLineCommand> m_drawLineCommands;
  std::vector<DrawScreenLineCommand> m_drawScreenLineCommands;
  std::vector<DrawScreenTextCommand> m_drawScreenTextCommands;
};
