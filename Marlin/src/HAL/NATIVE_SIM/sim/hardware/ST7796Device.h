#pragma once

#include <SDL2/SDL.h>
#include "../user_interface.h"

#include <list>
#include <deque>
#include "Gpio.h"

#include "XPT2046Device.h"

class ST7796Device: public Peripheral {
public:
  enum KeyName {
    KILL_BUTTON, ENCODER_BUTTON, COUNT
  };

  struct Command {
    Command(uint8_t cmd, std::vector<uint8_t> data) : cmd(cmd), data(data){};
    uint8_t cmd = 0;
    std::vector<uint8_t> data;
  };

  ST7796Device(pin_type clk, pin_type mosi, pin_type cs, pin_type dc, pin_type beeper, pin_type enc1, pin_type enc2, pin_type enc_but, pin_type kill);
  virtual ~ST7796Device();
  void process_command(Command cmd);
  void update();
  void interrupt(GpioEvent& ev);
  void ui_callback(UiWindow* window);

  pin_type clk_pin, mosi_pin, cs_pin, dc_pin, beeper_pin, enc1_pin, enc2_pin, enc_but_pin, kill_pin;

  uint8_t incomming_byte = 0;
  uint8_t incomming_bit_count = 0;
  uint8_t incomming_byte_count = 0;
  uint8_t incomming_cmd[3] = {};
  std::deque<Command> cmd_in;

  static constexpr uint32_t graphic_ram_size = 480 * 320;
  uint16_t graphic_ram[graphic_ram_size] = {}; // 64 x 256bit
  uint16_t graphic_ram_index = 0;
  uint16_t graphic_ram_index_x = 0, graphic_ram_index_y = 0;

  uint32_t address_counter = 0;
  int8_t address_increment = 1;

  uint16_t xMin = 0;
  uint16_t xMax = 0;
  uint16_t yMin = 0;
  uint16_t yMax = 0;

  bool key_pressed[KeyName::COUNT] = {};
  uint8_t encoder_position = 0.0f;
  static constexpr int8_t encoder_table[4] = {1, 3, 2, 0};

  bool dirty = true;
  std::chrono::steady_clock clock;
  std::chrono::steady_clock::time_point last_update;
  float scaler;
  GLuint texture_id;

  XPT2046Device touch;
};
