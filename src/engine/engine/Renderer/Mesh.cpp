#include <engine/Renderer/Mesh.h>

#include <glad/glad.h>

void Mesh::initialize(const Options& options) {
  vertexAttributes = options.attributes;
  vertexCount = options.vertexCount;
  indexFormat = options.indexFormat;
  indexCount = options.indexCount;

  glCreateVertexArrays(1, &gl.vao);
  GLuint relativeOffset = 0;
  for (uint32_t i = 0; i < vertexAttributes.size(); i++) {
    GLenum type = -1;
    GLuint typeSize = 0;
    switch (vertexAttributes[i].format) {
      case VertexAttribute::Format::f16:
        type = GL_HALF_FLOAT;
        typeSize = 2;
        break;
      case VertexAttribute::Format::f32:
        type = GL_FLOAT;
        typeSize = 4;
        break;
    }

    glEnableVertexArrayAttrib(gl.vao, i);
    glVertexArrayAttribFormat(gl.vao, i, vertexAttributes[i].dimension, type, GL_FALSE, relativeOffset);
    glVertexArrayAttribBinding(gl.vao, i, 0);
    relativeOffset += typeSize * vertexAttributes[i].dimension;
  }
  vertexSize = relativeOffset;

  glCreateBuffers(1, &gl.vertexBuffer);
  glNamedBufferStorage(gl.vertexBuffer, vertexSize * vertexCount, options.vertexBufferData, 0);
  glCreateBuffers(1, &gl.elementBuffer);
  glNamedBufferStorage(gl.elementBuffer, (options.indexFormat == IndexFormat::u32 ? 4 : 2) * indexCount, options.indexBufferData, 0);
  glVertexArrayVertexBuffer(gl.vao, 0, gl.vertexBuffer, 0, vertexSize);
  glVertexArrayElementBuffer(gl.vao, gl.elementBuffer);

  SubMesh subMesh;
  subMesh.indexStart = 0;
  subMesh.indexCount = options.indexCount;
  submeshes.push_back(subMesh);
}
