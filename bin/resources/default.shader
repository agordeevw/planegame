#ifdef VERTEX_SHADER
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 0) out vec3 position;
layout (location = 1) out vec3 normal;
layout (std140, binding = 2) uniform Material {
  vec3 color;
  vec3 ambient;
} material;
layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };
uniform mat4 model;
void main() {
  gl_Position = projection * view * model * vec4(inPosition, 1.0);
  position = (model * vec4(inPosition, 1.0)).xyz;
  normal = normalize(inNormal);
};
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 inNormal;
layout (location = 0) out vec4 fragColor;
struct Light { vec3 position; vec3 color; };
layout (std140, binding = 1) uniform Lights {
  vec3 positions[128];
  vec3 colors[128];
  int count;
} lights;
layout (std140, binding = 2) uniform Material {
  vec3 color;
  vec3 ambient;
} material;
uniform float time;
void main() {
  vec3 lightsColor = vec3(0.0,0.0,0.0);
  vec3 normal = normalize(inNormal);
  for (int i = 0; i < lights.count; i++) { lightsColor += lights.colors[i] * max(0.0, dot(normal, normalize(lights.positions[i] - position))); }
  fragColor = vec4(material.color * (material.ambient + lightsColor), 1.0);
};
#endif