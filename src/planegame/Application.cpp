#include <planegame/Application.h>
#include <planegame/Component/Camera.h>
#include <planegame/Component/Light.h>
#include <planegame/Component/MeshRenderer.h>
#include <planegame/Component/Script.h>
#include <planegame/Component/Transform.h>
#include <planegame/Object.h>
#include <planegame/Renderer/Material.h>
#include <planegame/Renderer/Mesh.h>
#include <planegame/Renderer/Shader.h>
#include <planegame/Renderer/Texture2D.h>
#include <planegame/Scene.h>
#include <planegame/Scripts/MovingObjectGeneratorScript.h>
#include <planegame/Scripts/PlaneChaseCameraScript.h>
#include <planegame/Scripts/PlaneControlScript.h>

#include <SDL.h>
#include <SDL_image.h>
#include <glad/glad.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

Application::~Application() {
  shutDown();
}

int Application::setUp() {
  if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
    return 1;
  if (IMG_Init(IMG_INIT_PNG) == 0)
    return 1;

  m_window = SDL_CreateWindow("",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    800, 600,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!m_window)
    return 1;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
  m_context = SDL_GL_CreateContext(m_window);
  if (!m_context)
    return 1;

  if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
    return 1;

  return 0;
}

void Application::shutDown() {
  if (m_shutdownCalled)
    return;

  m_shutdownCalled = true;

  if (m_context) {
    SDL_GL_DeleteContext(m_context);
    m_context = nullptr;
  }

  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }

  IMG_Quit();
  SDL_Quit();
}

void Application::importMesh(const char* filename, StringID sid) {
  uint32_t vertexCount;
  uint32_t indexCount;
  uint32_t submeshCount;
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  std::vector<uint32_t> submeshes;
  {
    FILE* file = fopen(filename, "rb");
    if (!file)
      throw std::runtime_error("file not found");

    if (fread(&vertexCount, sizeof(vertexCount), 1, file) != 1)
      throw std::runtime_error("import error");

    vertices.resize(vertexCount * 6);
    if (fread(vertices.data(), 6 * sizeof(float), vertexCount, file) != vertexCount)
      throw std::runtime_error("import error");

    if (fread(&indexCount, sizeof(indexCount), 1, file) != 1)
      throw std::runtime_error("import error");

    indices.resize(indexCount);
    if (fread(indices.data(), sizeof(uint32_t), indexCount, file) != indexCount)
      throw std::runtime_error("import error");

    if (fread(&submeshCount, sizeof(submeshCount), 1, file) != 1)
      throw std::runtime_error("import error");

    submeshes.resize(2 * submeshCount);
    if (fread(submeshes.data(), 2 * sizeof(uint32_t), submeshCount, file) != submeshCount)
      throw std::runtime_error("import error");

    fclose(file);
  }

  auto mesh = m_resources.create<Mesh>(sid);

  Mesh::Options options;
  options.attributes = {
    Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
    Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
  };
  options.indexBufferData = indices.data();
  options.indexCount = indexCount;
  options.indexFormat = Mesh::IndexFormat::u32;
  options.vertexBufferData = vertices.data();
  options.vertexCount = vertexCount;
  mesh->initialize(options);

  mesh->submeshes.resize(submeshCount);
  for (uint32_t i = 0; i < submeshCount; i++) {
    mesh->submeshes[i] = { submeshes[2 * i + 0], submeshes[2 * i + 1] };
  }
}

void Application::setUpResources() {
  nlohmann::json jResources;
  try {
    std::ifstream is("./resources.json");
    jResources = nlohmann::json::parse(is);
  }
  catch (const nlohmann::json::parse_error& e) {
    std::cerr << e.what() << std::endl;
    throw e;
  }

  if (jResources.contains("texture2d")) {
    nlohmann::json jTexture2ds = jResources.at("texture2d");
    for (nlohmann::json::iterator it = jTexture2ds.begin(); it != jTexture2ds.end(); ++it) {
      Texture2D::Options options;
      options.path = it.value().at("path").get<std::string>();
      options.magFilter = GL_NEAREST;
      options.minFilter = GL_NEAREST;
      auto texture = m_resources.create<Texture2D>(makeSID(it.key().c_str()));
      texture->initialize(options);
    }
  }

  if (jResources.contains("mesh")) {
    nlohmann::json jMeshes = jResources.at("mesh");
    for (nlohmann::json::iterator it = jMeshes.begin(); it != jMeshes.end(); ++it) {
      importMesh(it.value().at("path").get<std::string>().c_str(), makeSID(it.key().c_str()));
    }
  }

  {
    auto mesh = m_resources.create<Mesh>(SID("land"));
    Vertex vertices[]{
      Vertex{ { -10000.0f, 0.0f, -10000.0f }, { 0.0f, 1.0f, 0.0f } },
      Vertex{ { 10000.0f, 0.0f, -10000.0f }, { 0.0f, 1.0f, 0.0f } },
      Vertex{ { 10000.0f, 0.0f, 10000.0f }, { 0.0f, 1.0f, 0.0f } },
      Vertex{ { -10000.0f, 0.0f, 10000.0f }, { 0.0f, 1.0f, 0.0f } },
    };
    uint32_t indices[]{ 0, 2, 1, 0, 3, 2 };

    Mesh::Options options;
    options.attributes = {
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
    };
    options.indexBufferData = indices;
    options.indexCount = 6;
    options.indexFormat = Mesh::IndexFormat::u32;
    options.vertexBufferData = vertices;
    options.vertexCount = 4;
    mesh->initialize(options);
  }

  {
    auto mesh = m_resources.create<Mesh>(SID("object"));
    Vertex vertices[]{
      Vertex{ { -1.0, -1.0, 0.0 }, { 1.0, 0.0, 0.0 } },
      Vertex{ { 1.0, -1.0, 0.0 }, { 0.0, 1.0, 0.0 } },
      Vertex{ { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } }
    };
    uint32_t indices[]{ 0, 1, 2 };

    Mesh::Options options;
    options.attributes = {
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3),
      Mesh::VertexAttribute(Mesh::VertexAttribute::Format::f32, 3)
    };
    options.indexBufferData = indices;
    options.indexCount = 3;
    options.indexFormat = Mesh::IndexFormat::u32;
    options.vertexBufferData = vertices;
    options.vertexCount = 3;
    mesh->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("default"));
    Shader::Options options{};
    options.vertexSource = "layout (location = 0) in vec3 inPosition;\n"
                           "layout (location = 1) in vec3 inNormal;\n"
                           "layout (location = 0) out vec3 position;\n"
                           "layout (location = 1) out vec3 normal;\n"
                           "layout (std140, binding = 2) uniform Material {\n"
                           "  vec3 color;\n"
                           "  vec3 ambient;\n"
                           "} material;\n"
                           "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                           "uniform mat4 model;"
                           "void main() {\n"
                           "  gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
                           "  position = (model * vec4(inPosition, 1.0)).xyz;\n"
                           "  normal = normalize(inNormal);\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec3 position;\n"
                             "layout (location = 1) in vec3 inNormal;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "struct Light { vec3 position; vec3 color; };\n"
                             "layout (std140, binding = 1) uniform Lights {\n"
                             "  vec3 positions[128];\n"
                             "  vec3 colors[128];\n"
                             "  int count;\n"
                             "} lights;\n"
                             "layout (std140, binding = 2) uniform Material {\n"
                             "  vec3 color;\n"
                             "  vec3 ambient;\n"
                             "} material;\n"
                             "uniform float time;"
                             "void main() {\n"
                             "  vec3 lightsColor = vec3(0.0,0.0,0.0);\n"
                             "  vec3 normal = normalize(inNormal);\n"
                             "  for (int i = 0; i < lights.count; i++) { lightsColor += lights.colors[i] * max(0.0, dot(normal, normalize(lights.positions[i] - position))); }"
                             "  fragColor = vec4(material.color * (material.ambient + lightsColor), 1.0);\n"
                             "}";
    shader->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("default.land"));
    Shader::Options options{};
    options.vertexSource = "layout (location = 0) in vec3 inPosition;\n"
                           "layout (location = 1) in vec3 inNormal;\n"
                           "layout (location = 0) out vec3 position;\n"
                           "layout (location = 1) out vec3 normal;\n"
                           "layout (std140, binding = 2) uniform Material {\n"
                           "  vec3 color;\n"
                           "  vec3 ambient;\n"
                           "} material;\n"
                           "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                           "uniform mat4 model;"
                           "void main() {\n"
                           "  gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
                           "  position = (model * vec4(inPosition, 1.0)).xyz;\n"
                           "  normal = normalize(inNormal);\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec3 position;\n"
                             "layout (location = 1) in vec3 inNormal;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "struct Light { vec3 position; vec3 color; };\n"
                             "layout (std140, binding = 1) uniform Lights {\n"
                             "  vec3 positions[128];\n"
                             "  vec3 colors[128];\n"
                             "  int count;\n"
                             "} lights;\n"
                             "layout (std140, binding = 2) uniform Material {\n"
                             "  vec3 color;\n"
                             "  vec3 ambient;\n"
                             "} material;\n"
                             "uniform float time;"
                             "void main() {\n"
                             "  vec3 lightsColor = vec3(0.0,0.0,0.0);\n"
                             "  vec3 normal = normalize(inNormal);\n"
                             "  for (int i = 0; i < lights.count; i++) { lightsColor += lights.colors[i] * max(0.0, dot(normal, normalize(lights.positions[i] - position))); }"
                             "  vec3 materialColor = material.color;\n"
                             "  if ((int(0.05 * position.x) + int(0.05 * position.z)) % 2 == 0) materialColor *= 0.5;"
                             "  fragColor = vec4(materialColor * (material.ambient + lightsColor), 1.0);\n"
                             "}";
    shader->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("screenspace.sky"));
    Shader::Options options{};
    options.vertexSource = "layout (location = 0) out vec2 uv;\n"
                           "void main() {\n"
                           "  const vec2 verts[] = vec2[](vec2(-1.0, 1.0), vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));"
                           "  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);\n"
                           "  uv = verts[gl_VertexID];\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec2 uv;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "uniform vec3 cameraPosition;\n"
                             "uniform vec3 cameraUp;\n"
                             "uniform vec3 cameraForward;\n"
                             "uniform vec3 cameraRight;\n"
                             "uniform float aspectRatio;\n"
                             "uniform float fov;\n"
                             "vec3 skyColor(vec3 rayOrig, vec3 rayDir) {\n"
                             "  if (rayDir.y < 0.0) return vec3(0.0, 0.0, 0.0);\n"
                             "  return mix(vec3(0.9, 0.9, 0.9), vec3(0.1, 0.2, 0.5), sqrt(sqrt(rayDir.y)));"
                             "}\n"
                             "void main() {\n"
                             "  vec3 rayOrig = cameraPosition;\n"
                             "  vec3 rayDir = normalize(cameraForward + sin(0.5 * fov) * cameraUp * uv.y + cameraRight * uv.x * aspectRatio * sin(0.5 * fov));"
                             "  fragColor = vec4(skyColor(rayOrig, rayDir), 1.0);"
                             "}";
    shader->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("debug.drawline"));
    Shader::Options options{};
    options.vertexSource = "layout (std140, binding = 0) uniform ViewProjection { mat4 view; mat4 projection; };\n"
                           "uniform vec3 verts[2];\n"
                           "uniform vec3 color;\n"
                           "layout (location = 0) out vec3 outColor;\n"
                           "void main() {\n"
                           "  gl_Position = projection * view * vec4(verts[gl_VertexID], 1.0);\n"
                           "  outColor = color;\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec3 color;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "void main() {\n"
                             "  fragColor = vec4(color, 1.0);\n"
                             "}";
    shader->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("debug.drawscreenline"));
    Shader::Options options{};
    options.vertexSource = "uniform vec2 verts[2];\n"
                           "uniform vec3 color;\n"
                           "layout (location = 0) out vec3 outColor;\n"
                           "void main() {\n"
                           "  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);\n"
                           "  outColor = color;\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec3 color;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "void main() {\n"
                             "  fragColor = vec4(color, 1.0);\n"
                             "}";
    shader->initialize(options);
  }

  {
    auto shader = m_resources.create<Shader>(SID("debug.drawscreentext"));
    Shader::Options options{};
    options.vertexSource = "layout (location = 0) out vec2 outUV;\n"
                           "uniform vec2 screenPosition;\n"
                           "uniform vec2 screenCharSize;\n"
                           "uniform vec2 charBottomLeft;\n"
                           "uniform vec2 charSize;\n"
                           "void main() {\n"
                           "  const vec2 verts[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));\n"
                           "  gl_Position = vec4(screenPosition + screenCharSize * verts[gl_VertexID], 0.0, 1.0);\n"
                           "  outUV = charBottomLeft + charSize * verts[gl_VertexID];\n"
                           "}";
    options.fragmentSource = "layout (location = 0) in vec2 uv;\n"
                             "layout (location = 0) out vec4 fragColor;\n"
                             "layout (binding = 0) uniform sampler2D textureFont;\n"
                             "void main() {\n"
                             "  vec3 color = texture(textureFont, uv).xyz;"
                             "  if (color.y < 0.5) discard;\n"
                             "  fragColor = vec4(texture(textureFont, uv).xyz, 1.0);\n"
                             "}";
    shader->initialize(options);
  }

  {
    auto material = m_resources.create<Material>(SID("default.land"));
    material->initialize(m_resources.get<Shader>(SID("default.land")));
    material->setValue("Material.color", glm::vec3{ 0.4f, 0.2f, 0.1f });
  }

  {
    auto material = m_resources.create<Material>(SID("default.object"));
    material->initialize(m_resources.get<Shader>(SID("default")));
    material->setValue("Material.color", glm::vec3{ 1.0f, 1.0f, 1.0f });
  }

  {
    auto material = m_resources.create<Material>(SID("su37.body"));
    material->initialize(m_resources.get<Shader>(SID("default")));
    material->setValue("Material.color", glm::vec3{ 0.2f, 0.400000f, 0.200000f });
  }

  {
    auto material = m_resources.create<Material>(SID("su37.cockpit"));
    material->initialize(m_resources.get<Shader>(SID("default")));
    material->setValue("Material.color", glm::vec3{ 0.274425f, 0.282128f, 0.800000f });
  }

  {
    auto material = m_resources.create<Material>(SID("su37.engine"));
    material->initialize(m_resources.get<Shader>(SID("default")));
    material->setValue("Material.color", glm::vec3{ 0.100000f, 0.100000f, 0.100000f });
  }

  {
    auto material = m_resources.create<Material>(SID("sky"));
    material->initialize(m_resources.get<Shader>(SID("screenspace.sky")));
  }
}

void Application::setUpScene() {
  Object* movingObjectGeneratorObject = m_scene.makeObject();
  movingObjectGeneratorObject->addComponent<MovingObjectGeneratorScript>();

  Object* landObject = m_scene.makeObject();
  landObject->transform.position = { 0.0f, -10.0f, 0.0f };
  MeshRenderer* landObjectMeshRenderer = landObject->addComponent<MeshRenderer>();
  landObjectMeshRenderer->mesh = m_resources.get<Mesh>(SID("land"));
  landObjectMeshRenderer->materials.push_back(m_resources.get<Material>(SID("default.land")));

  Object* lights[2];
  lights[0] = m_scene.makeObject();
  lights[0]->transform.position = { 0.0, 5.0f, 0.0f };
  lights[0]->addComponent<Light>()->color = { 5.0f, 5.0f, 5.0f };
  lights[1] = m_scene.makeObject();
  lights[1]->transform.position = { 2.0, 2.0f, 0.0f };
  lights[1]->addComponent<Light>()->color = { 3.0f, 0.0f, 0.0f };

  Object* plane = m_scene.makeObject();
  {
    plane->tag = 0;
    plane->transform.position = { 0.0f, 10.0f, 0.0f };
    MeshRenderer* meshRenderer = plane->addComponent<MeshRenderer>();
    meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
    plane->addComponent<PlaneControlScript>();

    Object* childLight = m_scene.makeObject();
    plane->addChild(childLight);
    Light* light = childLight->addComponent<Light>();
    childLight->transform.position = { 0.0f, 0.0f, 4.0f };
    light->color = { 5.0f, 5.0f, 5.0f };
  }

  Object* plane1 = m_scene.makeObject();
  {
    plane1->transform.position = { 0.0f, 10.0f, 0.0f };
    MeshRenderer* meshRenderer = plane1->addComponent<MeshRenderer>();
    meshRenderer->mesh = m_resources.get<Mesh>(SID("su37"));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.body")));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.cockpit")));
    meshRenderer->materials.push_back(m_resources.get<Material>(SID("su37.engine")));
  }

  Object* mainCameraObject = m_scene.makeObject();
  {
    Camera* mainCamera = mainCameraObject->addComponent<Camera>();
    mainCamera->aspectRatio = float(m_width) / float(m_height);
    mainCamera->isMain = true;
    mainCamera->fov = glm::radians(70.0f);
    mainCameraObject->addComponent<PlaneChaseCameraScript>();
  }
}

void Application::run() {
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(glDebugCallback, this);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

  m_scene.scriptContext.input = &m_input;
  m_scene.scriptContext.random = &m_random;
  m_scene.scriptContext.time = &m_time;
  m_scene.scriptContext.scene = &m_scene;
  m_scene.scriptContext.resources = &m_resources;
  m_scene.scriptContext.debug = &m_debug;
  SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);

  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  GLuint color, depth, fbo;

  glCreateTextures(GL_TEXTURE_2D, 1, &color);
  glTextureStorage2D(color, 1, GL_SRGB8_ALPHA8, m_width, m_height);

  glCreateTextures(GL_TEXTURE_2D, 1, &depth);
  glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, m_width, m_height);

  glCreateFramebuffers(1, &fbo);
  glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
  glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
  }

  setUpResources();
  setUpScene();

  GLint ubOffsetAlignment;
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ubOffsetAlignment);

  struct MaterialUniformBufferStackAllocator {
  public:
    MaterialUniformBufferStackAllocator(uint32_t size, uint32_t alignment) : alignment(alignment), backingStorage(size) {}

    struct PtrOffsetPair {
      char* ptr;
      uint32_t offset;
    } alloc(uint32_t size) {
      uint32_t ret = nextAllocationStart;
      m_allocatedSize = nextAllocationStart + size;
      nextAllocationStart = (m_allocatedSize + alignment - 1) & (~(alignment - 1));
      return { &backingStorage[ret], ret };
    }

    void clear() {
      nextAllocationStart = 0;
    }

    uint32_t allocatedSize() const {
      return m_allocatedSize;
    }

    void* data() {
      return backingStorage.data();
    }

  private:
    uint32_t alignment;
    std::vector<char> backingStorage;
    uint32_t nextAllocationStart = 0;
    uint32_t m_allocatedSize = 0;
  } uboMaterialAllocator(64 * 1024, ubOffsetAlignment);

  const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  const uint32_t FRAME_0 = 0;
  const uint32_t FRAME_1 = 1;
  GLuint uniformBuffers[MAX_FRAMES_IN_FLIGHT][2];
  GLuint uboScene[MAX_FRAMES_IN_FLIGHT];
  GLuint uboMaterial[MAX_FRAMES_IN_FLIGHT];
  for (uint32_t frameIdx = 0; frameIdx < MAX_FRAMES_IN_FLIGHT; frameIdx++) {
    glCreateBuffers(2, uniformBuffers[frameIdx]);
    glNamedBufferData(uniformBuffers[frameIdx][0], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[frameIdx][1], 64 * 1024, nullptr, GL_DYNAMIC_DRAW);
    uboScene[frameIdx] = uniformBuffers[frameIdx][0];
    uboMaterial[frameIdx] = uniformBuffers[frameIdx][1];
  }
  GLuint vaoDebug;
  glCreateVertexArrays(1, &vaoDebug);

  SDL_GL_SetSwapInterval(0);
  SDL_ShowWindow(m_window);
  uint32_t currentFrame = 0;
  auto advanceToNextFrame = [&MAX_FRAMES_IN_FLIGHT, &currentFrame]() {
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  };

  SDL_bool relativeMouseMode = SDL_TRUE;
  SDL_SetRelativeMouseMode(relativeMouseMode);
  m_time.timeSinceStart = 0.0;
  uint64_t ticksLast = SDL_GetPerformanceCounter();
  uint64_t frequency = SDL_GetPerformanceFrequency();
  int counter = 0;
  float frameTimeAccum = 0.0f;
  while (true) {
    handleEvents();
    if (m_quit)
      break;

    uint64_t ticksNow = SDL_GetPerformanceCounter();
    uint64_t ticksDiff = ticksNow - ticksLast;
    double delta = double(ticksDiff) / double(frequency);
    m_time.timeSinceStart += delta;
    m_time.dt = float(delta);

    {
      counter++;
      frameTimeAccum += m_time.dt / float(60.0f);
      if (counter >= 60) {
        counter = 0;
        char title[1024];
        sprintf(title, "%f ms, %f FPS", 1000.0f * frameTimeAccum, (1.0f / frameTimeAccum));
        frameTimeAccum = 0.0f;
        SDL_SetWindowTitle(m_window, title);
      }
    }
    ticksLast = ticksNow;

    // Update
    {
      if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyPressed[SDL_SCANCODE_C]) {
        relativeMouseMode = relativeMouseMode == SDL_TRUE ? SDL_FALSE : SDL_TRUE;
        SDL_SetRelativeMouseMode(relativeMouseMode);
      }

      for (auto& script : m_scene.scripts.activeScripts) {
        script->update();
      }
      for (auto& script : m_scene.scripts.pendingScripts) {
        script->initialize();
        m_scene.scripts.activeScripts.push_back(std::move(script));
      }
      m_scene.scripts.pendingScripts.clear();

      m_scene.destroyObjects();
    }

    // Render

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearDepthf(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);

    struct SceneUniformBufferLayout {
      struct {
        glm::mat4 view;
        glm::mat4 projection;
      } matrices;
      char pad0[128];
      struct Lights {
        struct {
          glm::vec3 v;
          char pad[4];
        } positions[128];
        struct {
          glm::vec3 v;
          char pad[4];
        } colors[128];
        int32_t count;
        char pad[12];
      } lights;
    } sceneUBOLayout;

    // Set up scene uniform
    {
      sceneUBOLayout.matrices.view = m_scene.getMainCamera()->viewMatrix4();
      sceneUBOLayout.matrices.projection = m_scene.getMainCamera()->projectionMatrix4();
      uint32_t lightId = 0;
      for (auto& light : m_scene.components.get<Light>()) {
        sceneUBOLayout.lights.positions[lightId].v = light->transform.worldPosition();
        sceneUBOLayout.lights.colors[lightId].v = light->color;
        lightId++;
      }
      sceneUBOLayout.lights.count = lightId;
      glNamedBufferSubData(uboScene[currentFrame], 0, sizeof(sceneUBOLayout), &sceneUBOLayout);
      glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboScene[currentFrame], offsetof(SceneUniformBufferLayout, matrices), sizeof(sceneUBOLayout.matrices));
      glBindBufferRange(GL_UNIFORM_BUFFER, 1, uboScene[currentFrame], offsetof(SceneUniformBufferLayout, lights), sizeof(sceneUBOLayout.lights));
    }

    // Set up materials buffer
    std::unordered_map<const Material*, uint32_t> offsetForMaterial;
    uboMaterialAllocator.clear();
    {
      for (auto& meshRenderer : m_scene.components.get<MeshRenderer>()) {
        for (std::size_t i = 0; i < meshRenderer->mesh->submeshes.size(); i++) {
          const Material* material = meshRenderer->materials[i];
          if (offsetForMaterial.find(material) == offsetForMaterial.end()) {
            auto [ptr, offset] = uboMaterialAllocator.alloc(material->materialUniformBlock->size);
            offsetForMaterial[material] = offset;
            memcpy(ptr, material->uniformStorage.get(), material->materialUniformBlock->size);
          }
        }
      }

      glNamedBufferSubData(uboMaterial[currentFrame], 0, uboMaterialAllocator.allocatedSize(), uboMaterialAllocator.data());
    }

    for (auto& meshRenderer : m_scene.components.get<MeshRenderer>()) {
      glBindVertexArray(meshRenderer->mesh->gl.vao);
      for (std::size_t i = 0; i < meshRenderer->mesh->submeshes.size(); i++) {
        Mesh::SubMesh& submesh = meshRenderer->mesh->submeshes[i];
        const Material* material = meshRenderer->materials[i];
        glBindProgramPipeline(material->shader->programPipeline);
        glBindBufferRange(GL_UNIFORM_BUFFER, 2, uboMaterial[currentFrame], offsetForMaterial.at(material), material->materialUniformBlock->size);
        material->shader->vertShader.setUniform(SID("model"), meshRenderer->transform.asMatrix4());
        material->shader->vertShader.setUniform(SID("time"), float(m_time.timeSinceStart));
        material->shader->fragShader.setUniform(SID("time"), float(m_time.timeSinceStart));
        glDrawElements(GL_TRIANGLES, submesh.indexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(submesh.indexStart * sizeof(uint32_t)));
      }
    }

    // screenspace effect, post-render, specific depth func
    {
      glDepthFunc(GL_EQUAL);
      glBindVertexArray(vaoDebug);
      const Material* material = m_resources.get<Material>(SID("sky"));
      glBindProgramPipeline(material->shader->programPipeline);
      material->shader->fragShader.setUniform(SID("cameraPosition"), m_scene.getMainCamera()->transform.position);
      material->shader->fragShader.setUniform(SID("cameraForward"), m_scene.getMainCamera()->transform.forward());
      material->shader->fragShader.setUniform(SID("cameraUp"), m_scene.getMainCamera()->transform.up());
      material->shader->fragShader.setUniform(SID("cameraRight"), m_scene.getMainCamera()->transform.right());
      material->shader->fragShader.setUniform(SID("aspectRatio"), m_scene.getMainCamera()->aspectRatio);
      material->shader->fragShader.setUniform(SID("fov"), m_scene.getMainCamera()->fov);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glDepthFunc(GL_GREATER);
    }

    glDisable(GL_DEPTH_TEST);
    {
      Shader* lineShader = m_resources.get<Shader>(SID("debug.drawline"));
      glBindVertexArray(vaoDebug);
      glBindProgramPipeline(lineShader->programPipeline);
      for (auto& command : m_debug.m_drawLineCommands) {
        lineShader->vertShader.setUniformArray(SID("verts[0]"), 2, command.verts);
        lineShader->vertShader.setUniform(SID("color"), command.color);
        glDrawArrays(GL_LINES, 0, 2);
      }
    }

    {
      Shader* lineShader = m_resources.get<Shader>(SID("debug.drawscreenline"));
      glBindVertexArray(vaoDebug);
      glBindProgramPipeline(lineShader->programPipeline);
      for (auto& command : m_debug.m_drawScreenLineCommands) {
        lineShader->vertShader.setUniformArray(SID("verts[0]"), 2, command.verts);
        lineShader->vertShader.setUniform(SID("color"), command.color);
        glDrawArrays(GL_LINES, 0, 2);
      }
    }

    {
      Shader* screenTextShader = m_resources.get<Shader>(SID("debug.drawscreentext"));
      Texture2D* textTexture = m_resources.get<Texture2D>(SID("debug.font"));
      glBindVertexArray(vaoDebug);
      glBindProgramPipeline(screenTextShader->programPipeline);
      glBindTextures(0, 1, &textTexture->handle);

      const float charTextureWidth = 1.0f / 16.0f;
      const float charTextureHeight = 1.0f / 16.0f;
      const float charScreenWidth = 1.0f / 20.0f * (float(textTexture->width) / float(textTexture->height));
      const float charScreenHeight = 1.0f / 20.0f;

      for (auto& command : m_debug.m_drawScreenTextCommands) {
        float x = command.topleft.x;
        float y = command.topleft.y;
        for (const char* c = command.str.c_str(); *c != 0; c++) {
          uint32_t cx = *c % 16;
          uint32_t cy = 15 - *c / 16;
          screenTextShader->vertShader.setUniform(SID("screenPosition"), glm::vec2(x, y));
          screenTextShader->vertShader.setUniform(SID("screenCharSize"), glm::vec2(charScreenWidth, charScreenHeight));
          screenTextShader->vertShader.setUniform(SID("charBottomLeft"), glm::vec2(cx * charTextureWidth, cy * charTextureHeight));
          screenTextShader->vertShader.setUniform(SID("charSize"), glm::vec2(charTextureWidth, charTextureHeight));
          glDrawArrays(GL_TRIANGLES, 0, 6);
          x += charScreenWidth;
        }
      }
    }

    glBindVertexArray(0);
    glBindProgramPipeline(0);

    m_debug.clear();

    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
      glBlitFramebuffer(
        0, 0, m_width, m_height,
        0, 0, m_width, m_height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    SDL_GL_SwapWindow(m_window);
    advanceToNextFrame();
  }
}

void Application::handleEvents() {
  m_input.mousedx = 0.0;
  m_input.mousedy = 0.0;
  for (int scancode = 0; scancode < SDL_NUM_SCANCODES; scancode++) {
    m_input.keyPressed[scancode] = false;
    m_input.keyReleased[scancode] = false;
  }

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT: {
        m_quit = true;
        return;
      }
      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        if (event.key.repeat == 0) {
          auto scancode = event.key.keysym.scancode;
          m_input.prevKeyDown[scancode] = m_input.keyDown[scancode];
          m_input.keyDown[scancode] = event.key.state == SDL_PRESSED;
          m_input.keyPressed[scancode] = event.key.state == SDL_PRESSED;
          m_input.keyReleased[scancode] = event.key.state == SDL_RELEASED;
        }
        break;
      }
      case SDL_MOUSEMOTION: {
        m_input.mousedx = float(event.motion.xrel);
        m_input.mousedy = float(event.motion.yrel);
        break;
      }
      case SDL_WINDOWEVENT: {
        switch (event.window.event) {
          case SDL_WINDOWEVENT_RESIZED: {
            SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
            glViewport(0, 0, m_width, m_height);
          }
        }
      }
      default:
        break;
    }
  }
}
