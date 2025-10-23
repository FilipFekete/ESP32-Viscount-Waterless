#include "led_controller.h"
#include "debug.h"

LEDController::LEDController(uint8_t pin)
  : _pin(pin), _state(false) {
}

void LEDController::begin() {
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  DEBUG_PRINTF(LED, "LED initialized on pin %d\n", _pin);
}

void LEDController::on() {
  _state = true;
  digitalWrite(_pin, HIGH);
}

void LEDController::off() {
  _state = false;
  digitalWrite(_pin, LOW);
}

void LEDController::toggle() {
  _state = !_state;
  digitalWrite(_pin, _state ? HIGH : LOW);
}

void LEDController::setState(bool state) {
  _state = state;
  digitalWrite(_pin, state ? HIGH : LOW);
}

bool LEDController::isOn() const {
  return _state;
}