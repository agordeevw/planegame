#ifdef VERTEX_SHADER
layout (location = 0) out vec2 uv;
void main() {
  const vec2 verts[] = vec2[](vec2(-1.0, 1.0), vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));
  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
  uv = verts[gl_VertexID];
}
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 fragColor;
uniform vec3 cameraPosition;
uniform vec3 cameraUp;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform float aspectRatio;
uniform float fov;
vec3 skyColor(vec3 rayOrig, vec3 rayDir) {
  if (rayDir.y < 0.0) return vec3(0.0, 0.0, 0.0);
  return mix(vec3(0.9, 0.9, 0.9), vec3(0.1, 0.2, 0.5), sqrt(sqrt(rayDir.y)));
}
void main() {
  vec3 rayOrig = cameraPosition;
  vec3 rayDir = normalize(cameraForward + sin(0.5 * fov) * cameraUp * uv.y + cameraRight * uv.x * aspectRatio * sin(0.5 * fov));
  fragColor = vec4(skyColor(rayOrig, rayDir), 1.0);
}
#endif