#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

class LEDController {
public:
  explicit LEDController(uint8_t pin);

  void begin();
  void on();
  void off();
  void toggle();
  void setState(bool state);
  bool isOn() const;

private:
  uint8_t _pin;
  bool _state;
};

#endif // LED_CONTROLLER_H