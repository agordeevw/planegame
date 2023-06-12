#include <planegame/Renderer/Texture2D.h>

#include <SDL_image.h>
#include <SDL_surface.h>

#include <stdexcept>
#include <vector>

void Texture2D::initialize(const Options& options) {
  SDL_Surface* surf = IMG_Load(options.path);
  GLenum glformat = GL_RGB;
  switch (surf->format->BytesPerPixel) {
    case 1:
      glformat = GL_RED;
      break;
    case 2:
      glformat = GL_RG;
      break;
    case 3:
      glformat = GL_RGB;
      break;
    case 4:
      glformat = GL_RGBA;
      break;
    default:
      throw std::runtime_error("unknown image format");
  }

  std::vector<char> data(surf->h * surf->w * surf->format->BytesPerPixel);
  for (int r = 0; r < surf->h; r++) {
    memcpy(data.data() + (surf->h - r - 1) * surf->pitch, (char*)surf->pixels + r * surf->pitch, surf->pitch);
  }
  width = surf->w;
  height = surf->h;

  glCreateTextures(GL_TEXTURE_2D, 1, &handle);
  glTextureStorage2D(handle, 1, GL_RGB8, surf->w, surf->h);
  glTextureSubImage2D(handle, 0, 0, 0, surf->w, surf->h, glformat, GL_UNSIGNED_BYTE, data.data());
  glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, options.minFilter);
  glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, options.magFilter);
  SDL_FreeSurface(surf);
}
