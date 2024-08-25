#ifdef VERTEX_SHADER
layout (location = 0) out vec2 outUV;
uniform vec2 screenPosition;
uniform vec2 screenCharSize;
uniform vec2 charBottomLeft;
uniform vec2 charSize;
void main() {
  const vec2 verts[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
  gl_Position = vec4(screenPosition + screenCharSize * verts[gl_VertexID], 0.0, 1.0);
  outUV = charBottomLeft + charSize * verts[gl_VertexID];
}
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 fragColor;
layout (binding = 0) uniform sampler2D textureFont;
void main() {
  vec3 color = texture(textureFont, uv).xyz;
  if (color.y < 0.5) discard;
  fragColor = vec4(texture(textureFont, uv).xyz, 1.0);
}
#endif