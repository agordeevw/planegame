#ifdef VERTEX_SHADER
uniform vec2 verts[2];
uniform vec3 color;
layout (location = 0) out vec3 outColor;
void main() {
  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
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