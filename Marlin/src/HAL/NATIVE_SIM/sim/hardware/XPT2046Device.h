#pragma once

#include <SDL2/SDL.h>
#include "../user_interface.h"

#include <list>
#include <deque>
#include "Gpio.h"

class XPT2046Device: public Peripheral {
public:
  XPT2046Device(pin_type clk, pin_type mosi, pin_type cs, pin_type miso);
  virtual ~XPT2046Device();
  void update();
  void interrupt(GpioEvent& ev);
  void ui_callback(UiWindow* window);

  pin_type clk_pin, mosi_pin, cs_pin, miso_pin;

  uint8_t incomming_byte = 0;
  uint8_t incomming_bit_count = 0;
  uint8_t incomming_byte_count = 0;

  bool dirty = false;
  uint16_t clickX = 0;
  uint16_t clickY = 0;
  std::chrono::steady_clock clock;
  std::chrono::steady_clock::time_point last_update;
  float scaler;
};
