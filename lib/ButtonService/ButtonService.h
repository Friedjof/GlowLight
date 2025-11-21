#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <Arduino.h>
#include <Button2.h>

#include "GlowRegistry.h"

#include "GlowConfig.h"


class ButtonService {
public:
  ButtonService();
  ~ButtonService();

  void setup();
  void loop();

  void setSimpleClickHandler(void (*f)(Button2 &));
  void setDoubleClickHandler(void (*f)(Button2 &));
  void setLongClickHandler(void (*f)(Button2 &));

private:

  Button2 button;
};

#endif