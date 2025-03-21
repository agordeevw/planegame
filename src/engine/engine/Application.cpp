#include <engine/Application.h>
#include <engine/Component/Camera.h>
#include <engine/Component/Light.h>
#include <engine/Component/MeshRenderer.h>
#include <engine/Component/Script.h>
#include <engine/Component/Transform.h>
#include <engine/Object.h>
#include <engine/Renderer/Material.h>
#include <engine/Renderer/Mesh.h>
#include <engine/Renderer/Shader.h>
#include <engine/Renderer/Texture2D.h>
#include <engine/Scene.h>

#include <SDL.h>
#include <SDL_image.h>
#include <glad/glad.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

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
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
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

Mesh* Application::importMesh(const char* filename, StringID sid) {
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

  return mesh;
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
      m_mapObjectToName[texture] = it.key().c_str();
      texture->initialize(options);
    }
  }

  if (jResources.contains("mesh")) {
    nlohmann::json jMeshes = jResources.at("mesh");
    for (nlohmann::json::iterator it = jMeshes.begin(); it != jMeshes.end(); ++it) {
      Mesh* mesh = importMesh(it.value().at("path").get<std::string>().c_str(), makeSID(it.key().c_str()));
      m_mapObjectToName[mesh] = it.key().c_str();
    }
  }

  if (jResources.contains("shader")) {
    nlohmann::json jShaders = jResources.at("shader");
    for (nlohmann::json::iterator it = jShaders.begin(); it != jShaders.end(); ++it) {
      std::string source;
      {
        std::string path = it.value().at("path").get<std::string>();
        std::ifstream is(path);
        std::stringstream ss;
        ss << is.rdbuf();
        source = ss.str();
      }

      auto shader = m_resources.create<Shader>(makeSID(it.key().c_str()));
      Shader::Options options{};
      options.source = source.c_str();
      shader->initialize(options);
      m_mapObjectToName[shader] = it.key().c_str();
    }
  }

  if (jResources.contains("material")) {
    nlohmann::json jMaterials = jResources.at("material");
    for (nlohmann::json::iterator it = jMaterials.begin(); it != jMaterials.end(); ++it) {
      auto material = m_resources.create<Material>(makeSID(it.key().c_str()));
      material->initialize(m_resources.get<Shader>(makeSID(it.value().at("shader").get<std::string>().c_str())));

      nlohmann::json values = it.value().at("values");
      for (nlohmann::json::iterator itValue = values.begin(); itValue != values.end(); ++itValue) {
        const char* valueName = itValue.key().c_str();
        std::string valueTypeName = itValue.value().at(0).get<std::string>();
        if (valueTypeName == "vec3") {
          glm::vec3 v;
          v[0] = itValue.value().at(1).at(0).get<float>();
          v[1] = itValue.value().at(1).at(1).get<float>();
          v[2] = itValue.value().at(1).at(2).get<float>();
          material->setValue(valueName, v);
        }
        else {
          throw std::runtime_error("unknown valueTypeName: " + valueTypeName);
        }
      }

      m_mapObjectToName[material] = it.key().c_str();
    }
  }
}

void Application::setUpScene() {
  try {
    std::ifstream is("./scene.json");
    deserializeScene(is);
  }
  catch (const nlohmann::json::parse_error& e) {
    std::cerr << e.what() << std::endl;
    throw e;
  }
}

void Application::serializeScene(std::ostream& os) {
  nlohmann::json j;

  std::unordered_map<Object*, int> mapObjectToId;
  {
    int objectId = 0;
    for (const auto& object : m_scene.objects) {
      mapObjectToId[object.get()] = objectId++;
    }
  }

  j["objects"] = nlohmann::json::array();
  {
    for (const auto& object : m_scene.objects) {
      nlohmann::json jObject = {};
      jObject["id"] = mapObjectToId.at(object.get());
      if (object->parent()) {
        jObject["parent"] = mapObjectToId.at(object->parent());
      }
      auto transformPosition = nlohmann::json::array({
        object->transform.position[0],
        object->transform.position[1],
        object->transform.position[2],
      });
      auto transformRotation = nlohmann::json::array({
        object->transform.rotation[0],
        object->transform.rotation[1],
        object->transform.rotation[2],
        object->transform.rotation[3],
      });
      jObject["transform"] = nlohmann::json::array({ transformPosition, transformRotation });
      if (object->tag != -1) {
        jObject["tag"] = object->tag;
      }
      j["objects"].push_back(std::move(jObject));
    }
  }

  j["components"] = {};
  {
    std::vector<nlohmann::json> jCameras;
    for (const auto& camera : m_scene.components.cameras) {
      nlohmann::json jCamera;
      jCamera["object"] = mapObjectToId.at(&camera->object);
      jCamera["fov"] = camera->fov;
      jCamera["aspectRatio"] = camera->aspectRatio;
      jCamera["isMain"] = camera->isMain;
      jCameras.push_back(std::move(jCamera));
    }
    j["components"]["cameras"] = std::move(jCameras);
  }
  {
    std::vector<nlohmann::json> jLights;
    for (const auto& light : m_scene.components.lights) {
      nlohmann::json jLight;
      jLight["object"] = mapObjectToId.at(&light->object);
      jLight["type"] = int(light->type);
      jLight["color"] = nlohmann::json::array({
        light->color[0],
        light->color[1],
        light->color[2],
      });
      jLights.push_back(std::move(jLight));
    }
    j["components"]["lights"] = std::move(jLights);
  }
  {
    std::vector<nlohmann::json> jMeshRenderers;
    for (const auto& meshRenderer : m_scene.components.meshRenderers) {
      nlohmann::json jMeshRenderer;
      jMeshRenderer["object"] = mapObjectToId.at(&meshRenderer->object);
      jMeshRenderer["mesh"] = m_mapObjectToName.at(meshRenderer->mesh);
      jMeshRenderer["materials"] = nlohmann::json::array();
      for (const auto& material : meshRenderer->materials) {
        jMeshRenderer["materials"].push_back(m_mapObjectToName.at(material));
      }
      jMeshRenderers.push_back(std::move(jMeshRenderer));
    }
    j["components"]["meshRenderers"] = std::move(jMeshRenderers);
  }

  {
    std::vector<nlohmann::json> jScripts;
    for (const auto& script : m_scene.scripts.activeScripts) {
      nlohmann::json jScript;
      jScript["object"] = mapObjectToId.at(&script->object);
      jScript["type"] = script->getName();
      // TODO: save properties of the script.
      jScripts.push_back(std::move(jScript));
    }
    j["components"]["scripts"] = std::move(jScripts);
  }

  os << j.dump(2, ' ', false);
}

void Application::deserializeScene(std::istream& is) {
  m_scene.clear();

  nlohmann::json j;
  is >> j;

  std::unordered_map<int, Object*> mapIdToObject;
  for (nlohmann::json::iterator it = j.at("objects").begin(); it != j.at("objects").end(); ++it) {
    Object* object = m_scene.makeObject();
    mapIdToObject[it.value().at("id").get<int>()] = object;
    object->transform.position[0] = it.value().at("transform").at(0).at(0).get<float>();
    object->transform.position[1] = it.value().at("transform").at(0).at(1).get<float>();
    object->transform.position[2] = it.value().at("transform").at(0).at(2).get<float>();
    object->transform.rotation[0] = it.value().at("transform").at(1).at(0).get<float>();
    object->transform.rotation[1] = it.value().at("transform").at(1).at(1).get<float>();
    object->transform.rotation[2] = it.value().at("transform").at(1).at(2).get<float>();
    object->transform.rotation[3] = it.value().at("transform").at(1).at(3).get<float>();
    if (it.value().contains("tag")) {
      object->tag = uint32_t(it.value().at("tag").get<int>());
    }
  }
  for (nlohmann::json::iterator it = j.at("objects").begin(); it != j.at("objects").end(); ++it) {
    if (it.value().contains("parent")) {
      mapIdToObject.at(it.value().at("parent").get<int>())->addChild(mapIdToObject.at(it.value().at("id").get<int>()));
    }
  }

  for (nlohmann::json::iterator it = j.at("components").at("cameras").begin(); it != j.at("components").at("cameras").end(); ++it) {
    auto* component = mapIdToObject.at(it.value().at("object").get<int>())->addComponent<Camera>();
    component->fov = it.value().at("fov").get<float>();
    component->aspectRatio = it.value().at("aspectRatio").get<float>();
    component->isMain = it.value().at("isMain").get<bool>();
  }

  for (nlohmann::json::iterator it = j.at("components").at("lights").begin(); it != j.at("components").at("lights").end(); ++it) {
    auto* component = mapIdToObject.at(it.value().at("object").get<int>())->addComponent<Light>();
    component->type = LightType(it.value().at("type").get<int>());
    component->color[0] = it.value().at("color").at(0).get<float>();
    component->color[1] = it.value().at("color").at(1).get<float>();
    component->color[2] = it.value().at("color").at(2).get<float>();
  }

  for (nlohmann::json::iterator it = j.at("components").at("meshRenderers").begin(); it != j.at("components").at("meshRenderers").end(); ++it) {
    auto* component = mapIdToObject.at(it.value().at("object").get<int>())->addComponent<MeshRenderer>();
    component->mesh = m_resources.get<Mesh>(makeSID(it.value().at("mesh").get<std::string>().c_str()));
    for (nlohmann::json::iterator itMat = it.value().at("materials").begin(); itMat != it.value().at("materials").end(); ++itMat) {
      component->materials.push_back(m_resources.get<Material>(makeSID(itMat.value().get<std::string>().c_str())));
    }
  }

  for (nlohmann::json::iterator it = j.at("components").at("scripts").begin(); it != j.at("components").at("scripts").end(); ++it) {
    Object* object = mapIdToObject.at(it.value().at("object").get<int>());
    Script* script = createScript(it.value().at("type").get<std::string>().c_str(), *object);
    object->attachScript(script);
  }
}

void Application::run() {
  IMGUI_CHECKVERSION();
  struct ScopedImGuiContext {
    ScopedImGuiContext(ImGuiContext* ctx) : m_ctx(ctx) {}

    ~ScopedImGuiContext() {
      ImGui::DestroyContext(m_ctx);
    }

    ImGuiContext* m_ctx;
  } scopedCtx(ImGui::CreateContext());
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().DisplaySize.x = float(m_width);
  ImGui::GetIO().DisplaySize.y = float(m_height);
  ImGui::StyleColorsDark();

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

  GLuint tex2dImguiFont;
  {
    unsigned char* fontPixels{};
    int fontWidth{};
    int fontHeight{};
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex2dImguiFont);
    glTextureParameteri(tex2dImguiFont, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex2dImguiFont, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex2dImguiFont, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex2dImguiFont, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(tex2dImguiFont, 1, GL_RGBA8, fontWidth, fontHeight);
    glTextureSubImage2D(tex2dImguiFont, 0, 0, 0, fontWidth, fontHeight, GL_RGBA, GL_UNSIGNED_BYTE, fontPixels);
  }

  GLuint vboImguiVertexBuffer;
  GLuint vboImguiIndexBuffer;
  GLuint vaoImgui;
  int imguiVertexBufferSize = 0;
  int imguiIndexBufferSize = 0;
  {
    glCreateBuffers(1, &vboImguiVertexBuffer);
    glCreateBuffers(1, &vboImguiIndexBuffer);
    glCreateVertexArrays(1, &vaoImgui);
    glVertexArrayVertexBuffer(vaoImgui, 0, vboImguiVertexBuffer, 0, sizeof(ImDrawVert));
    glVertexArrayAttribFormat(vaoImgui, 0, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, ImDrawVert::pos));
    glVertexArrayAttribFormat(vaoImgui, 1, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, ImDrawVert::uv));
    glVertexArrayAttribFormat(vaoImgui, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, ImDrawVert::col));
    glVertexArrayAttribBinding(vaoImgui, 0, 0);
    glVertexArrayAttribBinding(vaoImgui, 1, 0);
    glVertexArrayAttribBinding(vaoImgui, 2, 0);
    glEnableVertexArrayAttrib(vaoImgui, 0);
    glEnableVertexArrayAttrib(vaoImgui, 1);
    glEnableVertexArrayAttrib(vaoImgui, 2);
    glVertexArrayElementBuffer(vaoImgui, vboImguiIndexBuffer);
  }

  GLuint color, depth, fbo;
  auto createMainFramebuffer = [&]() {
    glCreateTextures(GL_TEXTURE_2D, 1, &color);
    glCreateTextures(GL_TEXTURE_2D, 1, &depth);
    glCreateFramebuffers(1, &fbo);

    glTextureStorage2D(color, 1, GL_SRGB8_ALPHA8, m_width, m_height);
    glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, m_width, m_height);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
    glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);
  };

  createMainFramebuffer();

  auto recreateMainFramebuffer = [&]() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, 0, 0);
    glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, 0, 0);

    glDeleteTextures(1, &color);
    glDeleteTextures(1, &depth);
    glDeleteFramebuffers(1, &fbo);

    createMainFramebuffer();
  };

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
  }

  loadScriptLoader();
  setUpResources();
  setUpScene();

  GLint ubOffsetAlignment;
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ubOffsetAlignment);

  const std::size_t UNIFORM_BUFFER_SIZE = 64 * 1024;

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
  } uboMaterialAllocator(UNIFORM_BUFFER_SIZE, ubOffsetAlignment);

  const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  const uint32_t FRAME_0 = 0;
  const uint32_t FRAME_1 = 1;
  GLuint uniformBuffers[MAX_FRAMES_IN_FLIGHT][2];
  GLuint uboScene[MAX_FRAMES_IN_FLIGHT];
  GLuint uboMaterial[MAX_FRAMES_IN_FLIGHT];
  for (uint32_t frameIdx = 0; frameIdx < MAX_FRAMES_IN_FLIGHT; frameIdx++) {
    glCreateBuffers(2, uniformBuffers[frameIdx]);
    glNamedBufferData(uniformBuffers[frameIdx][0], UNIFORM_BUFFER_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glNamedBufferData(uniformBuffers[frameIdx][1], UNIFORM_BUFFER_SIZE, nullptr, GL_DYNAMIC_DRAW);
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

      if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyDown[SDL_SCANCODE_LSHIFT] && m_input.keyPressed[SDL_SCANCODE_S]) {
        std::ofstream f("scene.json");
        serializeScene(f);
      }
      if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyDown[SDL_SCANCODE_LSHIFT] && m_input.keyPressed[SDL_SCANCODE_L]) {
        std::ifstream f("scene.json");
        deserializeScene(f);
      }
      if (m_input.keyDown[SDL_SCANCODE_LCTRL] && m_input.keyDown[SDL_SCANCODE_LSHIFT] && m_input.keyPressed[SDL_SCANCODE_R]) {
        std::vector<std::pair<std::string, Object*>> scriptObjectPairs;
        for (const auto& script : m_scene.scripts.activeScripts) {
          scriptObjectPairs.push_back({ script->getName(), &script->object });
        }
        for (const auto& script : m_scene.scripts.pendingScripts) {
          scriptObjectPairs.push_back({ script->getName(), &script->object });
        }

        m_scene.scripts.activeScripts.clear();
        m_scene.scripts.pendingScripts.clear();
        unloadScriptLoader();
        loadScriptLoader();

        for (const auto& scriptObjectPair : scriptObjectPairs) {
          Script* script = createScript(scriptObjectPair.first.c_str(), *scriptObjectPair.second);
          scriptObjectPair.second->attachScript(script);
        }
      }
    }

    // Render

    if (m_resizeWindow) {
      SDL_GL_GetDrawableSize(m_window, &m_width, &m_height);
      ImGui::GetIO().DisplaySize.x = float(m_width);
      ImGui::GetIO().DisplaySize.y = float(m_height);
      recreateMainFramebuffer();
      glViewport(0, 0, m_width, m_height);
      m_resizeWindow = false;
    }

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
        } directions[128];
        struct {
          glm::vec3 v;
          char pad[4];
        } colors[128];
        struct {
          int v;
          char pad[12];
        } types[128];
        int32_t count;
        char pad[12];
      } lights;
    } sceneUBOLayout;
    static_assert(sizeof(SceneUniformBufferLayout) <= UNIFORM_BUFFER_SIZE);

    // Set up scene uniform
    {
      sceneUBOLayout.matrices.view = m_scene.getMainCamera()->viewMatrix4();
      sceneUBOLayout.matrices.projection = m_scene.getMainCamera()->projectionMatrix4();
      uint32_t lightId = 0;
      for (auto& light : m_scene.components.get<Light>()) {
        sceneUBOLayout.lights.positions[lightId].v = light->transform.worldPosition();
        sceneUBOLayout.lights.directions[lightId].v = light->transform.forward();
        sceneUBOLayout.lights.colors[lightId].v = light->color;
        sceneUBOLayout.lights.types[lightId].v = int(light->type);
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

    // UI

    ImGui::GetIO().DisplaySize.x = float(m_width);
    ImGui::GetIO().DisplaySize.y = float(m_height);

    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData->Valid) {
      GLboolean blendEnabled = glIsEnabled(GL_BLEND);
      GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
      GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
      GLboolean stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
      GLboolean scissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_CULL_FACE);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_STENCIL_TEST);
      glEnable(GL_SCISSOR_TEST);

      int totalVertexBufferSize = drawData->TotalVtxCount * int(sizeof(ImDrawVert));
      if (imguiVertexBufferSize < totalVertexBufferSize) {
        imguiVertexBufferSize = 2 * totalVertexBufferSize;
        glDeleteBuffers(1, &vboImguiVertexBuffer);
        glCreateBuffers(1, &vboImguiVertexBuffer);
        glVertexArrayVertexBuffer(vaoImgui, 0, vboImguiVertexBuffer, 0, sizeof(ImDrawVert));
        glNamedBufferStorage(vboImguiVertexBuffer, imguiVertexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
      }
      int totalIndexBufferSize = drawData->TotalIdxCount * int(sizeof(ImDrawIdx));
      if (imguiIndexBufferSize < totalIndexBufferSize) {
        imguiIndexBufferSize = 2 * totalIndexBufferSize;
        glDeleteBuffers(1, &vboImguiIndexBuffer);
        glCreateBuffers(1, &vboImguiIndexBuffer);
        glVertexArrayElementBuffer(vaoImgui, vboImguiIndexBuffer);
        glNamedBufferStorage(vboImguiIndexBuffer, imguiIndexBufferSize, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
      }

      void* vertexBufferData = glMapNamedBufferRange(
        vboImguiVertexBuffer,
        0,
        totalVertexBufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
      void* indexBufferData = glMapNamedBufferRange(
        vboImguiIndexBuffer,
        0,
        totalIndexBufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
      glBindVertexArray(vaoImgui);
      glBindTextures(0, 1, &tex2dImguiFont);

      glViewport(0, 0, (GLsizei)m_width, (GLsizei)m_height);
      float L = drawData->DisplayPos.x;
      float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
      float T = drawData->DisplayPos.y;
      float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
      const glm::mat4 orthoProjection = {
        { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { (R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f },
      };
      Shader* imguiShader = m_resources.get<Shader>(SID("imgui"));
      imguiShader->vertShader.setUniform(SID("proj"), orthoProjection);
      glBindProgramPipeline(imguiShader->programPipeline);

      int numDrawnVertices = 0;
      int numDrawnIndices = 0;
      int baseVertex = 0;
      int baseIndex = 0;
      for (ImDrawList* drawList : drawData->CmdLists) {
        baseVertex = numDrawnVertices;
        baseIndex = numDrawnIndices;

        memcpy(
          (char*)vertexBufferData + numDrawnVertices * sizeof(ImDrawVert),
          drawList->VtxBuffer.Data,
          drawList->VtxBuffer.size() * sizeof(ImDrawVert));
        numDrawnVertices += drawList->VtxBuffer.size();

        memcpy(
          (char*)indexBufferData + numDrawnIndices * sizeof(ImDrawIdx),
          drawList->IdxBuffer.Data,
          drawList->IdxBuffer.size() * sizeof(ImDrawIdx));
        numDrawnIndices += drawList->IdxBuffer.size();

        for (const ImDrawCmd& cmd : drawList->CmdBuffer) {
          glDrawElementsBaseVertex(GL_TRIANGLES,
            cmd.ElemCount, GL_UNSIGNED_SHORT, (void*)((baseIndex + cmd.IdxOffset) * sizeof(ImDrawIdx)), baseVertex);
        }
      }
      glUnmapNamedBuffer(vboImguiIndexBuffer);
      glUnmapNamedBuffer(vboImguiVertexBuffer);
      blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
      cullFaceEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
      depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
      stencilTestEnabled ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
      scissorTestEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
      glBindVertexArray(0);
      glBindProgramPipeline(0);
      glBindTextures(0, 0, nullptr);
    }

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

namespace {
ImGuiKey ImGui_ImplSDL2_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode) {
  switch (keycode) {
    case SDLK_TAB:
      return ImGuiKey_Tab;
    case SDLK_LEFT:
      return ImGuiKey_LeftArrow;
    case SDLK_RIGHT:
      return ImGuiKey_RightArrow;
    case SDLK_UP:
      return ImGuiKey_UpArrow;
    case SDLK_DOWN:
      return ImGuiKey_DownArrow;
    case SDLK_PAGEUP:
      return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN:
      return ImGuiKey_PageDown;
    case SDLK_HOME:
      return ImGuiKey_Home;
    case SDLK_END:
      return ImGuiKey_End;
    case SDLK_INSERT:
      return ImGuiKey_Insert;
    case SDLK_DELETE:
      return ImGuiKey_Delete;
    case SDLK_BACKSPACE:
      return ImGuiKey_Backspace;
    case SDLK_SPACE:
      return ImGuiKey_Space;
    case SDLK_RETURN:
      return ImGuiKey_Enter;
    case SDLK_ESCAPE:
      return ImGuiKey_Escape;
    // case SDLK_QUOTE: return ImGuiKey_Apostrophe;
    case SDLK_COMMA:
      return ImGuiKey_Comma;
    // case SDLK_MINUS: return ImGuiKey_Minus;
    case SDLK_PERIOD:
      return ImGuiKey_Period;
    // case SDLK_SLASH: return ImGuiKey_Slash;
    case SDLK_SEMICOLON:
      return ImGuiKey_Semicolon;
    // case SDLK_EQUALS: return ImGuiKey_Equal;
    // case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
    // case SDLK_BACKSLASH: return ImGuiKey_Backslash;
    // case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
    // case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
    case SDLK_CAPSLOCK:
      return ImGuiKey_CapsLock;
    case SDLK_SCROLLLOCK:
      return ImGuiKey_ScrollLock;
    case SDLK_NUMLOCKCLEAR:
      return ImGuiKey_NumLock;
    case SDLK_PRINTSCREEN:
      return ImGuiKey_PrintScreen;
    case SDLK_PAUSE:
      return ImGuiKey_Pause;
    case SDLK_KP_0:
      return ImGuiKey_Keypad0;
    case SDLK_KP_1:
      return ImGuiKey_Keypad1;
    case SDLK_KP_2:
      return ImGuiKey_Keypad2;
    case SDLK_KP_3:
      return ImGuiKey_Keypad3;
    case SDLK_KP_4:
      return ImGuiKey_Keypad4;
    case SDLK_KP_5:
      return ImGuiKey_Keypad5;
    case SDLK_KP_6:
      return ImGuiKey_Keypad6;
    case SDLK_KP_7:
      return ImGuiKey_Keypad7;
    case SDLK_KP_8:
      return ImGuiKey_Keypad8;
    case SDLK_KP_9:
      return ImGuiKey_Keypad9;
    case SDLK_KP_PERIOD:
      return ImGuiKey_KeypadDecimal;
    case SDLK_KP_DIVIDE:
      return ImGuiKey_KeypadDivide;
    case SDLK_KP_MULTIPLY:
      return ImGuiKey_KeypadMultiply;
    case SDLK_KP_MINUS:
      return ImGuiKey_KeypadSubtract;
    case SDLK_KP_PLUS:
      return ImGuiKey_KeypadAdd;
    case SDLK_KP_ENTER:
      return ImGuiKey_KeypadEnter;
    case SDLK_KP_EQUALS:
      return ImGuiKey_KeypadEqual;
    case SDLK_LCTRL:
      return ImGuiKey_LeftCtrl;
    case SDLK_LSHIFT:
      return ImGuiKey_LeftShift;
    case SDLK_LALT:
      return ImGuiKey_LeftAlt;
    case SDLK_LGUI:
      return ImGuiKey_LeftSuper;
    case SDLK_RCTRL:
      return ImGuiKey_RightCtrl;
    case SDLK_RSHIFT:
      return ImGuiKey_RightShift;
    case SDLK_RALT:
      return ImGuiKey_RightAlt;
    case SDLK_RGUI:
      return ImGuiKey_RightSuper;
    case SDLK_APPLICATION:
      return ImGuiKey_Menu;
    case SDLK_0:
      return ImGuiKey_0;
    case SDLK_1:
      return ImGuiKey_1;
    case SDLK_2:
      return ImGuiKey_2;
    case SDLK_3:
      return ImGuiKey_3;
    case SDLK_4:
      return ImGuiKey_4;
    case SDLK_5:
      return ImGuiKey_5;
    case SDLK_6:
      return ImGuiKey_6;
    case SDLK_7:
      return ImGuiKey_7;
    case SDLK_8:
      return ImGuiKey_8;
    case SDLK_9:
      return ImGuiKey_9;
    case SDLK_a:
      return ImGuiKey_A;
    case SDLK_b:
      return ImGuiKey_B;
    case SDLK_c:
      return ImGuiKey_C;
    case SDLK_d:
      return ImGuiKey_D;
    case SDLK_e:
      return ImGuiKey_E;
    case SDLK_f:
      return ImGuiKey_F;
    case SDLK_g:
      return ImGuiKey_G;
    case SDLK_h:
      return ImGuiKey_H;
    case SDLK_i:
      return ImGuiKey_I;
    case SDLK_j:
      return ImGuiKey_J;
    case SDLK_k:
      return ImGuiKey_K;
    case SDLK_l:
      return ImGuiKey_L;
    case SDLK_m:
      return ImGuiKey_M;
    case SDLK_n:
      return ImGuiKey_N;
    case SDLK_o:
      return ImGuiKey_O;
    case SDLK_p:
      return ImGuiKey_P;
    case SDLK_q:
      return ImGuiKey_Q;
    case SDLK_r:
      return ImGuiKey_R;
    case SDLK_s:
      return ImGuiKey_S;
    case SDLK_t:
      return ImGuiKey_T;
    case SDLK_u:
      return ImGuiKey_U;
    case SDLK_v:
      return ImGuiKey_V;
    case SDLK_w:
      return ImGuiKey_W;
    case SDLK_x:
      return ImGuiKey_X;
    case SDLK_y:
      return ImGuiKey_Y;
    case SDLK_z:
      return ImGuiKey_Z;
    case SDLK_F1:
      return ImGuiKey_F1;
    case SDLK_F2:
      return ImGuiKey_F2;
    case SDLK_F3:
      return ImGuiKey_F3;
    case SDLK_F4:
      return ImGuiKey_F4;
    case SDLK_F5:
      return ImGuiKey_F5;
    case SDLK_F6:
      return ImGuiKey_F6;
    case SDLK_F7:
      return ImGuiKey_F7;
    case SDLK_F8:
      return ImGuiKey_F8;
    case SDLK_F9:
      return ImGuiKey_F9;
    case SDLK_F10:
      return ImGuiKey_F10;
    case SDLK_F11:
      return ImGuiKey_F11;
    case SDLK_F12:
      return ImGuiKey_F12;
    case SDLK_F13:
      return ImGuiKey_F13;
    case SDLK_F14:
      return ImGuiKey_F14;
    case SDLK_F15:
      return ImGuiKey_F15;
    case SDLK_F16:
      return ImGuiKey_F16;
    case SDLK_F17:
      return ImGuiKey_F17;
    case SDLK_F18:
      return ImGuiKey_F18;
    case SDLK_F19:
      return ImGuiKey_F19;
    case SDLK_F20:
      return ImGuiKey_F20;
    case SDLK_F21:
      return ImGuiKey_F21;
    case SDLK_F22:
      return ImGuiKey_F22;
    case SDLK_F23:
      return ImGuiKey_F23;
    case SDLK_F24:
      return ImGuiKey_F24;
    case SDLK_AC_BACK:
      return ImGuiKey_AppBack;
    case SDLK_AC_FORWARD:
      return ImGuiKey_AppForward;
    default:
      break;
  }

  // Fallback to scancode
  switch (scancode) {
    case SDL_SCANCODE_GRAVE:
      return ImGuiKey_GraveAccent;
    case SDL_SCANCODE_MINUS:
      return ImGuiKey_Minus;
    case SDL_SCANCODE_EQUALS:
      return ImGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET:
      return ImGuiKey_LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:
      return ImGuiKey_RightBracket;
    case SDL_SCANCODE_NONUSBACKSLASH:
      return ImGuiKey_Backslash;
    case SDL_SCANCODE_SEMICOLON:
      return ImGuiKey_Semicolon;
    case SDL_SCANCODE_APOSTROPHE:
      return ImGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA:
      return ImGuiKey_Comma;
    case SDL_SCANCODE_PERIOD:
      return ImGuiKey_Period;
    case SDL_SCANCODE_SLASH:
      return ImGuiKey_Slash;
    default:
      break;
  }
  return ImGuiKey_None;
}
} // namespace

void Application::handleEvents() {
  m_input.mousedx = 0.0;
  m_input.mousedy = 0.0;
  for (int scancode = 0; scancode < SDL_NUM_SCANCODES; scancode++) {
    m_input.keyPressed[scancode] = false;
    m_input.keyReleased[scancode] = false;
  }

  ImGui::GetIO();

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
          SDL_Scancode scancode = event.key.keysym.scancode;
          m_input.prevKeyDown[scancode] = m_input.keyDown[scancode];
          m_input.keyDown[scancode] = event.key.state == SDL_PRESSED;
          m_input.keyPressed[scancode] = event.key.state == SDL_PRESSED;
          m_input.keyReleased[scancode] = event.key.state == SDL_RELEASED;
          ImGui::GetIO().AddKeyEvent(
            ImGui_ImplSDL2_KeyEventToImGuiKey(event.key.keysym.sym, scancode),
            event.key.state == SDL_PRESSED);
        }
        break;
      }
      case SDL_MOUSEMOTION: {
        m_input.mousedx = float(event.motion.xrel);
        m_input.mousedy = float(event.motion.yrel);
        ImGui::GetIO().AddMouseSourceEvent(ImGuiMouseSource::ImGuiMouseSource_Mouse);
        ImGui::GetIO().AddMousePosEvent((float)event.motion.x, (float)event.motion.y);
        break;
      }
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        int mouseButton = -1;
        switch (event.button.button) {
          case SDL_BUTTON_LEFT:
            mouseButton = 0;
            break;
          case SDL_BUTTON_RIGHT:
            mouseButton = 1;
            break;
          case SDL_BUTTON_MIDDLE:
            mouseButton = 2;
            break;
        }
        if (mouseButton == -1) {
          break;
        }
        ImGui::GetIO().AddMouseSourceEvent(ImGuiMouseSource::ImGuiMouseSource_Mouse);
        ImGui::GetIO().AddMouseButtonEvent(mouseButton, event.type == SDL_MOUSEBUTTONDOWN);
        break;
      }
      case SDL_TEXTINPUT: {
        ImGui::GetIO().AddInputCharactersUTF8(event.text.text);
        break;
      }
      case SDL_WINDOWEVENT: {
        switch (event.window.event) {
          case SDL_WINDOWEVENT_RESIZED: {
            m_resizeWindow = true;
            break;
          }
        }
      }
      default:
        break;
    }
  }
}
