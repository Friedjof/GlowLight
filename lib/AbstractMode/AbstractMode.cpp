#include "AbstractMode.h"

AbstractMode::AbstractMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) {
  this->lightService = lightService;
  this->distanceService = distanceService;
  this->communicationService = communicationService;

  this->registry = GlowRegistry();
}

// meta functions
String AbstractMode::getTitle() {
  return this->title;
}

String AbstractMode::getDescription() {
  return this->description;
}

String AbstractMode::getAuthor() {
  return this->author;
}

String AbstractMode::getContact() {
  return this->contact;
}

String AbstractMode::getVersion() {
  return this->version;
}

String AbstractMode::getLicense() {
  return this->license;
}

// option functions
uint8_t AbstractMode::getCurrentOption() {
  return this->currentOption;
}

uint8_t AbstractMode::getNumberOfOptions() {
  return this->options.size();
}

bool AbstractMode::addOption(String title, std::function<void()> callback, bool alert, bool onlyOnce, bool disabled) {
  option_t option;

  option.title = title;
  option.callback = callback;
  option.alert = alert;
  option.onlyOnce = onlyOnce;
  option.disabled = disabled;

  this->options.add(option);

  return true;
}

bool AbstractMode::nextOption() {
  if (this->options.size() == 0) {
    Serial.println("[DEBUG] No options available");
    return false;
  }

  this->currentOption++;

  if (this->currentOption >= this->options.size()) {
    this->currentOption = 0;
  }

  Serial.print("[INFO] Switched to option '");
  Serial.print(this->options.get(this->currentOption).title);
  Serial.println("'");

  this->optionChanged = true;
  this->optionCalled = false;

  return this->options.get(this->currentOption).alert;
}

bool AbstractMode::setOption(uint8_t option) {
  if (this->options.size() == 0 || option >= this->options.size()) {
    return false;
  }

  this->currentOption = option;

  this->optionChanged = true;
  this->optionCalled = false;

  return this->options.get(this->currentOption).alert;
}

bool AbstractMode::callCurrentOption() {
  if (this->options.size() == 0 || this->currentOption >= this->options.size()) {
    return false;
  }

  if ((this->optionCalled && this->options.get(this->currentOption).onlyOnce) || this->options.get(this->currentOption).disabled) {
    return false;
  }

  this->options.get(this->currentOption).callback();

  this->optionCalled = true;

  return true;
}

bool AbstractMode::recallCurrentOption() {
  if (this->options.size() == 0 || this->currentOption >= this->options.size()) {
    return false;
  }

  this->optionCalled = true;

  this->options.get(this->currentOption).callback();

  return true;
}

bool AbstractMode::optionHasChanged() {
  if (this->optionChanged) {
    this->optionChanged = false;
    return true;
  }

  return false;
}

void AbstractMode::sendCommand(const String& command, const JsonDocument& payload) {
  if (this->communicationService == nullptr) {
    return;
  }

  this->communicationService->sendModeCommand(this->title, this->version, command, payload);
}

// brightness functions
bool AbstractMode::updateBrightnessFromSensor() {
  if ((!this->distanceService->isObjectPresent() && !this->distanceService->hasObjectDisappeared() && !this->distanceService->hasWipeDetected()) || this->distanceService->fixed()) {
    return false;
  }

  if (this->currentResult.level != this->lastResult.level) {
    uint8_t brightness = 0;

    if (!this->distanceService->hasWipeDetected()) {
      brightness = this->expNormalize(this->currentResult.level, 0, DISTANCE_LEVELS, LED_MAX_BRIGHTNESS, .5);
    } else {
      if (this->desiredBrightness == LED_MIN_BRIGHTNESS) {
        brightness = LED_MAX_BRIGHTNESS;
      } else {
        brightness = LED_MIN_BRIGHTNESS;
      }
    }

    this->lastResult = this->currentResult;
    return this->setDesiredBrightness(brightness);
  }

  return false;
}

void AbstractMode::applyDesiredBrightness() {
  this->applyHardwareBrightness(this->desiredBrightness);
}

void AbstractMode::applyHardwareBrightness(uint8_t brightness) {
  this->lightService->setHardwareBrightness(brightness);
}

bool AbstractMode::setDesiredBrightness(uint8_t brightness) {
  bool changed = brightness != this->desiredBrightness;

  this->desiredBrightness = brightness;
  this->applyDesiredBrightness();

  return changed;
}

uint8_t AbstractMode::getDesiredBrightness() {
  return this->desiredBrightness;
}

// distance functions
uint16_t AbstractMode::getLevel() {
  return this->currentResult.level;
}

uint16_t AbstractMode::getDistance() {
  return this->currentResult.distance;
}

uint16_t AbstractMode::expNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor) {
  double normalized = (double)(input - min) / (max - min);
  double expPart = exp(normalized * log(levels));
  double linearPart = normalized * levels;
  return (uint16_t)((1.0 - factor) * linearPart + factor * expPart);
}

uint16_t AbstractMode::invExpNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor) {
  double normalized = (double)(input - min) / (max - min);
  double expPart = levels * (1.0 - exp(-normalized * log(levels)));
  double linearPart = normalized * levels;
  return (uint16_t)((1.0 - factor) * linearPart + factor * expPart);
}

// serialize and deserialize
JsonDocument AbstractMode::serialize() {
  this->registry.setInt("currentOption", this->currentOption);
  this->registry.setInt("brightness", this->desiredBrightness);

  return this->registry.serialize();
}

void AbstractMode::deserialize(JsonDocument doc) {
  // check if the title and version match (to prevent deserialization of wrong data for another mode)
  this->registry.deserialize(doc);

  this->currentOption = this->registry.getInt("currentOption");
  this->desiredBrightness = this->registry.getInt("brightness");

  // call the setup function of the derived class
  this->optionChanged = true;
  this->optionCalled = false;

  this->applyDesiredBrightness();

  Serial.println("[DEBUG] Deserialized data");
}

// main functions
void AbstractMode::loop() {
  this->currentResult = this->distanceService->getResult();

  this->callCurrentOption();

  this->customLoop();
}

void AbstractMode::first() {
  this->applyDesiredBrightness();
  this->lightService->setLightUpdateSteps(LED_UPDATE_STEPS);

  this->customFirst();
}

void AbstractMode::modeSetup() {
  // call the setup function of the derived class
  this->setup();

  this->registry.setTitle(this->getTitle());
  this->registry.setVersion(this->getVersion());

  // initialize the registry
  uint8_t maximumOption = this->getNumberOfOptions() == 0 ? 0 : this->getNumberOfOptions() - 1;
  this->registry.init("currentOption", RegistryType::INT, 0, 0, maximumOption);
  this->registry.init("brightness", RegistryType::INT, LED_DEFAULT_BRIGHTNESS, 0, LED_MAX_BRIGHTNESS);
}

void AbstractMode::applyRemoteUpdate(uint16_t distance, uint16_t level) {
  // Default: Apply as brightness
  // Convert level to brightness using exponential normalization
  uint8_t brightness = this->expNormalize(level, 0, DISTANCE_LEVELS, LED_MAX_BRIGHTNESS, 0.5);
  this->setDesiredBrightness(brightness);
}

bool AbstractMode::handleRemoteCommand(const String& command, const JsonDocument& payload) {
  return false;
}
