#include "AbstractMode.h"

AbstractMode::AbstractMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService) {
  this->lightService = lightService;
  this->distanceService = distanceService;
  this->communicationService = communicationService;
}

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

bool AbstractMode::setBrightness() {
  if (!this->distanceService->isObjectPresent() || this->distanceService->fixed()) {
    return false;
  }

  if (this->currentResult.level != this->lastResult.level) {
    uint16_t brightness = this->expNormalize(this->currentResult.level, 0, DISTANCE_LEVELS, LED_MAX_BRIGHTNESS, .5);

    this->lightService->setBrightness(brightness);

    this->lastResult = this->currentResult;
    this->brightness = brightness;

    return true;
  }

  return false;
}

bool AbstractMode::resetBrightness() {
  this->lightService->setBrightness(this->brightness);

  return true;
}


// set variables
void AbstractMode::setVar(String key, String value) {
  this->registry[key] = value;
}

void AbstractMode::setVar(String key, uint16_t value) {
  this->registry[key] = value;
}

void AbstractMode::setVar(String key, CRGB value) {
  String str = "CRGB(" + String(value.r) + ", " + String(value.g) + ", " + String(value.b) + ")";
  this->registry[key] = str;
}

// get variables
String AbstractMode::getStringVar(String key) {
  return this->registry[key].as<String>();
}

uint16_t AbstractMode::getUInt16Var(String key) {
  return this->registry[key].as<uint16_t>();
}

double AbstractMode::getDoubleVar(String key) {
  return this->registry[key].as<double>();
}

bool AbstractMode::getBoolVar(String key) {
  return this->registry[key].as<bool>();
}

CRGB AbstractMode::getCRGBVar(String key) {
  String str = this->registry[key].as<String>();

  int start = str.indexOf("(") + 1;
  int end = str.indexOf(")");

  String values = str.substring(start, end);

  int firstComma = values.indexOf(",");
  int secondComma = values.indexOf(",", firstComma + 1);

  int r = values.substring(0, firstComma).toInt();
  int g = values.substring(firstComma + 1, secondComma).toInt();
  int b = values.substring(secondComma + 1).toInt();

  return CRGB(r, g, b);
}


// update brightness
bool AbstractMode::updateBrightness(uint16_t brightness) {
  if (brightness == this->brightness) {
    return false;
  }

  this->brightness = brightness;

  this->lightService->setBrightness(this->brightness);

  return true;
}

uint16_t AbstractMode::getBrightness() {
  return this->brightness;
}

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

JsonDocument AbstractMode::serialize() {
  JsonDocument doc;

  this->registry["currentOption"] = this->currentOption;
  this->registry["brightness"] = this->brightness;

  doc["registry"] = this->registry;

  doc["title"] = this->title;
  doc["version"] = this->version;

  return doc;
}

void AbstractMode::deserialize(JsonDocument doc) {
  // check if the title and version match (to prevent deserialization of wrong data for another mode)
  if (doc["title"].as<String>() != this->title) {
    Serial.print("[ERROR] The mode title '");
    Serial.print(doc["title"].as<String>());
    Serial.print("' does not match with this mode title '");
    Serial.print(this->title);
    Serial.println("'. Skipping deserialization");
    return;
  } else if (doc["version"].as<String>() != this->version) {
    Serial.println("[ERROR] Mode Version does not match. Skipping deserialization");
    return;
  }

  this->registry = doc["registry"];

  uint16_t currentOption = this->registry["currentOption"].as<uint8_t>();
  uint16_t brightness = this->registry["brightness"].as<uint16_t>();

  // check if the current option is valid
  if (currentOption < this->options.size() && currentOption >= 0) {
    this->currentOption = currentOption;
  } else {
    Serial.println("[ERROR] Invalid option index. Setting to 0");
    this->currentOption = 0;
  }

  // check if the brightness is valid
  if (brightness >= 0 && brightness <= LED_MAX_BRIGHTNESS) {
    this->updateBrightness(brightness);
  } else {
    Serial.println("[ERROR] Invalid brightness value. Setting to default");
    this->updateBrightness(LED_DEFAULT_BRIGHTNESS);
  }

  // call the setup function of the derived class
  this->optionChanged = true;
  this->optionCalled = false;

  Serial.println("[DEBUG] Deserialized data");
}

void AbstractMode::loop() {
  this->currentResult = this->distanceService->getResult();

  this->callCurrentOption();

  this->customLoop();
}

void AbstractMode::first() {
  this->resetBrightness();
  this->lightService->setLightUpdateSteps(LED_UPDATE_STEPS);

  this->customFirst();
}

void AbstractMode::modeSetup() {
  // call the setup function of the derived class
  this->setup();
}
