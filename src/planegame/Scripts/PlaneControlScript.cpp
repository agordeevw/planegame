#include <planegame/Scripts/PlaneControlScript.h>
#include <planegame/Scripts/ScriptIncludes.h>

PlaneControlScript::PlaneControlScript(Object& object) : Script(object) {
  SCRIPT_REGISTER_PROPERTY(velocityShiftRate);
  SCRIPT_REGISTER_PROPERTY(minSpeed);
  SCRIPT_REGISTER_PROPERTY(maxPitchSpeed);
  SCRIPT_REGISTER_PROPERTY(maxRollSpeed);
  SCRIPT_REGISTER_PROPERTY(maxYawSpeed);
  SCRIPT_REGISTER_PROPERTY(pitchAcceleration);
  SCRIPT_REGISTER_PROPERTY(rollAcceleration);
  SCRIPT_REGISTER_PROPERTY(yawAcceleration);
  SCRIPT_REGISTER_PROPERTY(currentPitchSpeed);
  SCRIPT_REGISTER_PROPERTY(currentRollSpeed);
  SCRIPT_REGISTER_PROPERTY(currentYawSpeed);
  SCRIPT_REGISTER_PROPERTY(targetSpeed);
}

void PlaneControlScript::initialize() {
  velocity = targetSpeed * transform.forward();
}

inline void PlaneControlScript::update() {
  Transform& transform = object.transform;

  const glm::vec3 forward = transform.forward();
  const glm::vec3 up = transform.up();
  const glm::vec3 right = transform.right();
  const glm::vec3 globalUp = glm::vec3{ 0.0f, 1.0f, 0.0f };

  bool stalling = false;

  float inputThrustAcceleration = 0.0f;
  if (input().keyDown[SDL_SCANCODE_LSHIFT]) {
    inputThrustAcceleration = 100.0f;
  }
  else if (input().keyDown[SDL_SCANCODE_LCTRL]) {
    inputThrustAcceleration = -100.0f;
  }

  if (inputThrustAcceleration != 0.0f) {
    currentThrust += inputThrustAcceleration * time().dt;
  }
  else {
    if (currentThrust > baseThrust) {
      currentThrust = glm::clamp(currentThrust - 50.0f * time().dt, baseThrust, currentThrust);
    }
    else {
      currentThrust = glm::clamp(currentThrust + 50.0f * time().dt, currentThrust, baseThrust);
    }
  }
  currentThrust = glm::clamp(currentThrust, minThrust, maxThrust);

  float weight = 20.0f;
  float lift = 20.0f;
  if (targetSpeed < 15.0f) {
    stalling = true;
    lift = 10.0f - (15.0f - targetSpeed);
  }

  glm::vec3 linearAcceleration = forward * currentThrust - weight * globalUp + lift * up;
  targetSpeed = 20.0f * (glm::length(linearAcceleration) / baseThrust);

  // estimate linear acceleration vector which is used to determine base rotation speed

  float forwardDot = glm::dot(forward, linearAcceleration);
  float upDot = glm::dot(up, linearAcceleration);
  float rightDot = glm::dot(right, linearAcceleration);

  // velocity vector must become aligned with linear acceleration

  glm::vec3 targetVelocity = targetSpeed * glm::normalize(linearAcceleration);
  glm::vec3 deltaVelocity = targetVelocity - velocity;
  velocity += velocityShiftRate * deltaVelocity * time().dt;
  velocity = targetSpeed * glm::normalize(velocity);
  transform.position += velocity * time().dt;

  // rotation speed based on plane position
  float basePitchSpeed = 0.0f;
  float baseRollSpeed = 0.0f;
  float baseYawSpeed = 0.0f;
  {
    basePitchSpeed = 0.0025f * upDot;
    baseYawSpeed = -0.001f * rightDot;
  }

  // rotation acceleration based on player input
  float inputPitchAcceleration = 0.0f;
  float inputRollAcceleration = 0.0f;
  float inputYawAcceleration = 0.0f;

  if (!stalling) {
    if (input().keyDown[SDL_SCANCODE_W]) {
      inputPitchAcceleration = -pitchAcceleration;
    }
    if (input().keyDown[SDL_SCANCODE_S]) {
      inputPitchAcceleration = +pitchAcceleration;
    }
    if (input().keyDown[SDL_SCANCODE_D]) {
      inputRollAcceleration = -rollAcceleration;
    }
    if (input().keyDown[SDL_SCANCODE_A]) {
      inputRollAcceleration = +rollAcceleration;
    }
    if (input().keyDown[SDL_SCANCODE_E]) {
      inputYawAcceleration = -yawAcceleration;
    }
    if (input().keyDown[SDL_SCANCODE_Q]) {
      inputYawAcceleration = +yawAcceleration;
    }
  }

  if (inputPitchAcceleration != 0.0f) {
    currentPitchSpeed += inputPitchAcceleration * time().dt;
  }
  else {
    if (currentPitchSpeed > basePitchSpeed) {
      currentPitchSpeed = glm::clamp(currentPitchSpeed - 2.0f * time().dt, basePitchSpeed, currentPitchSpeed);
    }
    else {
      currentPitchSpeed = glm::clamp(currentPitchSpeed + 2.0f * time().dt, currentPitchSpeed, basePitchSpeed);
    }
  }

  if (inputRollAcceleration != 0.0f) {
    float acceleration = inputRollAcceleration;
    if (currentRollSpeed < baseRollSpeed && inputRollAcceleration > 0.0) {
      acceleration = 8.0f;
    }
    if (currentRollSpeed > baseRollSpeed && inputRollAcceleration < 0.0) {
      acceleration = -8.0f;
    }
    currentRollSpeed += acceleration * time().dt;
  }
  else {
    if (currentRollSpeed > baseRollSpeed) {
      currentRollSpeed = glm::clamp(currentRollSpeed - 8.0f * time().dt, baseRollSpeed, currentRollSpeed);
    }
    else {
      currentRollSpeed = glm::clamp(currentRollSpeed + 8.0f * time().dt, currentRollSpeed, baseRollSpeed);
    }
  }

  if (inputYawAcceleration != 0.0f) {
    currentYawSpeed += inputYawAcceleration * time().dt;
  }
  else {
    if (currentYawSpeed > baseYawSpeed) {
      currentYawSpeed = glm::clamp(currentYawSpeed - 0.4f * time().dt, baseYawSpeed, currentYawSpeed);
    }
    else {
      currentYawSpeed = glm::clamp(currentYawSpeed + 0.4f * time().dt, currentYawSpeed, baseYawSpeed);
    }
  }

  currentPitchSpeed = glm::clamp(currentPitchSpeed, -maxPitchSpeed, maxPitchSpeed);
  currentRollSpeed = glm::clamp(currentRollSpeed, -maxRollSpeed, maxRollSpeed);
  currentYawSpeed = glm::clamp(currentYawSpeed, -maxYawSpeed, maxYawSpeed);

  transform.rotateLocal(glm::vec3{ 1.0f, 0.0f, 0.0f }, currentPitchSpeed * time().dt);
  transform.rotateLocal(glm::vec3{ 0.0f, 0.0f, 1.0f }, currentRollSpeed * time().dt);
  transform.rotateLocal(glm::vec3{ 0.0f, 1.0f, 0.0f }, currentYawSpeed * time().dt);

  // nose direction
  {
    glm::vec3 forwardInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), forward);
    const float aspectRatio = scene().getMainCamera()->aspectRatio;
    const float fovy = scene().getMainCamera()->fov;
    forwardInViewSpace.x /= (-forwardInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
    forwardInViewSpace.y /= (-forwardInViewSpace.z * glm::sin(fovy * 0.5f));
    glm::vec2 forwardHint{};
    forwardHint.x = forwardInViewSpace.x;
    forwardHint.y = forwardInViewSpace.y;

    debug().drawScreenLine(glm::vec2{ -0.05f, 0.05f } + forwardHint, glm::vec2{ 0.0f, 0.0f } + forwardHint, { 0.0f, 0.7f, 0.0f });
    debug().drawScreenLine(glm::vec2{ 0.0f, 0.0f } + forwardHint, glm::vec2{ 0.05f, 0.05f } + forwardHint, { 0.0f, 0.7f, 0.0f });
  }

  {
    auto q = transform.rotation;
    // (heading, pitch, bank)
    // YXZ
    float headingAngle = std::atan2(2.0f * (q[0] * q[2] + q[1] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
    float pitchAngle = std::asin(2.0f * (q[2] * q[3] - q[0] * q[1]));
    float rollAngle = std::atan2(2.0f * (q[1] * q[2] + q[0] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[3] * q[3]));

    // pitch ladder
    for (int angle = -40; angle <= 40; angle += 10) {
      glm::vec2 horizonHint{};
      {
        glm::vec3 horizonInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), glm::normalize(glm::vec3{ forward.x, glm::sin(glm::radians(float(angle))), forward.z }));
        const float aspectRatio = scene().getMainCamera()->aspectRatio;
        const float fovy = scene().getMainCamera()->fov;
        horizonInViewSpace.x /= (-horizonInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
        horizonInViewSpace.y /= (-horizonInViewSpace.z * glm::sin(fovy * 0.5f));
        horizonHint.x = horizonInViewSpace.x;
        horizonHint.y = horizonInViewSpace.y;
      }

      glm::vec2 jej{ forward.x, forward.z };
      {
        glm::mat2x2 m{};
        m[0][0] = glm::cos(0.2f);
        m[0][1] = -glm::sin(0.2f);
        m[1][0] = glm::sin(0.2f);
        m[1][1] = glm::cos(0.2f);
        jej = m * jej;
      }
      glm::vec2 horizonShiftHint{};
      {
        glm::vec3 horizonShiftInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), glm::normalize(glm::vec3{ jej.x, glm::sin(glm::radians(float(angle))), jej.y }));
        const float aspectRatio = scene().getMainCamera()->aspectRatio;
        const float fovy = scene().getMainCamera()->fov;
        horizonShiftInViewSpace.x /= (-horizonShiftInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
        horizonShiftInViewSpace.y /= (-horizonShiftInViewSpace.z * glm::sin(fovy * 0.5f));
        horizonShiftHint.x = horizonShiftInViewSpace.x;
        horizonShiftHint.y = horizonShiftInViewSpace.y;
      }

      debug().drawScreenLine(horizonHint, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
      if (angle == 0) {
        debug().drawScreenLine(horizonHint + glm::vec2{ 0.0, 0.01 }, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
        debug().drawScreenLine(horizonHint - glm::vec2{ 0.0, 0.01 }, horizonShiftHint, { 0.0f, 0.8f, 0.0f });
      }
    }

    char buf[256];
    sprintf(buf, "heading: %f", headingAngle / float(M_PI) * 180.0f);
    debug().drawScreenText({ -1.0f, 0.7f }, buf);
    sprintf(buf, "pitch: %f", pitchAngle / float(M_PI) * 180.0f);
    debug().drawScreenText({ -1.0f, 0.9f }, buf);
    sprintf(buf, "roll : %f", rollAngle / float(M_PI) * 180.0f);
    debug().drawScreenText({ -1.0f, 0.8f }, buf);
  }

  // velocity vector hint
  {
    glm::vec3 velocityInViewSpace = glm::rotate(glm::inverse(scene().getMainCamera()->transform.rotation), velocity);
    const float aspectRatio = scene().getMainCamera()->aspectRatio;
    const float fovy = scene().getMainCamera()->fov;
    velocityInViewSpace.x /= (-velocityInViewSpace.z * aspectRatio * glm::sin(fovy * 0.5f));
    velocityInViewSpace.y /= (-velocityInViewSpace.z * glm::sin(fovy * 0.5f));
    glm::vec2 velocityHint{};
    velocityHint.x = velocityInViewSpace.x;
    velocityHint.y = velocityInViewSpace.y;

    debug().drawScreenLine(glm::vec2{ -0.05f, 0.0f } + velocityHint, glm::vec2{ +0.05f, 0.0f } + velocityHint, glm::vec3{ 0.0f, 1.0f, 0.0f });
    debug().drawScreenLine(glm::vec2{ 0.0f, 0.0f } + velocityHint, glm::vec2{ 0.0f, 0.05f } + velocityHint, glm::vec3{ 0.0f, 1.0f, 0.0f });
  }

  char buf[256];
  sprintf(buf, "%f", glm::length(velocity));
  debug().drawScreenText({ -0.5f, 0.3f }, buf);
  sprintf(buf, "%f", velocity.x);
  debug().drawScreenText({ -0.5f, 0.2f }, buf);
  sprintf(buf, "%f", velocity.y);
  debug().drawScreenText({ -0.5f, 0.1f }, buf);
  sprintf(buf, "%f", velocity.z);
  debug().drawScreenText({ -0.5f, 0.0f }, buf);
  sprintf(buf, "%f", transform.position.y);
  debug().drawScreenText({ +0.5f, 0.3f }, buf);
}
