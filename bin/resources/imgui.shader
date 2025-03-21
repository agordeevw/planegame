#ifdef VERTEX_SHADER
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 col;
layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outCol;
uniform mat4 proj;
void main() {
  gl_Position = proj * vec4(pos.xy, 0.0, 1.0);
  outUV = uv;
  outCol = col;
}
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 col;
layout (location = 0) out vec4 fragColor;
layout (binding = 0) uniform sampler2D textureFont;
void main() {
  vec4 texColor = texture(textureFont, uv);
  fragColor = texColor * col;
}
#endif