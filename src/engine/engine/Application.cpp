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
