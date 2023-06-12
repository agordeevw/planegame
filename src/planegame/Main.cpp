#include <planegame/Application.h>

#include <SDL_main.h>

int main(int, char**) {
  Application app;
  if (app.setUp() == 0) {
    app.run();
    app.shutDown();
  }

  return 0;
}
