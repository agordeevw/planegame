#ifdef VERTEX_SHADER
layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };
uniform vec3 verts[2];
uniform vec3 color;
layout (location = 0) out vec3 outColor;
void main() {
  gl_Position = projection * view * vec4(verts[gl_VertexID], 1.0);
  outColor = color;
}
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) in vec3 color;
layout (location = 0) out vec4 fragColor;
void main() {
  fragColor = vec4(color, 1.0);
}
#endif