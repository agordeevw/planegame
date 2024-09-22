#pragma once
#include <engine/StringID.h>

#include <cstdint>
#include <vector>

using GLuint = unsigned int;

class Mesh {
public:
  struct VertexAttribute {
    enum class Format {
      Unknown,
      f16,
      f32,
    };

    VertexAttribute(Format format, int dimension) : format(format), dimension(dimension) {}

    // enum class Type {
    //   Unknown,
    //   Position,
    //   Normal,
    //   Tangent,
    //   Color,
    //   UV,
    // };

    // Type type = Type::Unknown;
    Format format = Format::Unknown;
    int dimension = -1;
  };

  struct SubMesh {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
  };

  enum class IndexFormat {
    Unknown,
    u16,
    u32,
  };

  struct Options {
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    const void* vertexBufferData = nullptr;
    const void* indexBufferData = nullptr;
    std::vector<VertexAttribute> attributes;
    IndexFormat indexFormat = IndexFormat::Unknown;
  };

  void initialize(const Options& options);

  std::vector<SubMesh> submeshes;
  std::vector<VertexAttribute> vertexAttributes;
  uint32_t vertexCount = 0;
  IndexFormat indexFormat = IndexFormat::Unknown;
  uint32_t indexCount = 0;
  uint32_t vertexSize = 0;

  struct Mesh_GL {
    GLuint vao = -1;
    GLuint vertexBuffer = -1;
    GLuint elementBuffer = -1;
  } gl;
};
