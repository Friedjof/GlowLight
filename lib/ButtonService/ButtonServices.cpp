#include "ButtonService.h"

ButtonService::ButtonService() {
}

ButtonService::~ButtonService() {
}

void ButtonService::setup() {
  this->button.setLongClickTime(1000);
  this->button.setDoubleClickTime(500);
  this->button.begin(BUTTON_PIN, INPUT_PULLUP);
}

void ButtonService::loop() {
  this->button.loop();
}

void ButtonService::setSimpleClickHandler(void (*f)(Button2 &)) {
  this->button.setClickHandler(f);
}

void ButtonService::setDoubleClickHandler(void (*f)(Button2 &)) {
  this->button.setDoubleClickHandler(f);
}

void ButtonService::setLongClickHandler(void (*f)(Button2 &)) {
  this->button.setLongClickHandler(f);
}
