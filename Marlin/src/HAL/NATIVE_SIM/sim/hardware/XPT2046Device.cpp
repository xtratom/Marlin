#ifdef __PLAT_NATIVE_SIM__

#include <mutex>
#include <fstream>
#include <cmath>
#include <random>
#include "Gpio.h"

#include <GL/glew.h>
#include <GL/gl.h>

#include "XPT2046Device.h"
#include "../../tft/xpt2046.h"


XPT2046Device::XPT2046Device(pin_type clk, pin_type mosi, pin_type cs, pin_type miso)
  : clk_pin(clk), mosi_pin(mosi), miso_pin(miso), cs_pin(cs) {

  Gpio::attach(clk_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(cs_pin, [this](GpioEvent& event){ this->interrupt(event); });
  // Gpio::attach(miso_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(mosi_pin, [this](GpioEvent& event){ this->interrupt(event); });
}

XPT2046Device::~XPT2046Device() {}

void XPT2046Device::update() {
  static uint16_t buffer[480*320] = {};
  auto now = clock.now();
  float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_update).count();

  // if (dirty && delta > 1.0 / 30.0) {
  //   last_update = now;
  // }
}

uint8_t current_state = 0;

void trasmit(pin_type miso_pin, uint16_t value, uint8_t incomming_byte_count, uint8_t incomming_bit_count) {
  if (incomming_byte_count == 0)
    Gpio::pin_map[miso_pin].value = !!((value >> (15 - incomming_bit_count)) & 1);
  else if (incomming_byte_count == 1)
    Gpio::pin_map[miso_pin].value = !!((value >> (7 - incomming_bit_count)) & 1);
  else
    Gpio::pin_map[miso_pin].value = 0;
}

void XPT2046Device::interrupt(GpioEvent& ev) {
  if (ev.pin_id == mosi_pin && Gpio::pin_map[cs_pin].value == 0) {
    if (current_state == XPT2046_X) {
      trasmit(miso_pin, clickX, incomming_byte_count, incomming_bit_count);
    }
    else if (current_state == XPT2046_Y) {
      trasmit(miso_pin, clickY, incomming_byte_count, incomming_bit_count);
      dirty = false;
    }
    else if (current_state == XPT2046_Z1 && dirty) {
      trasmit(miso_pin, XPT2046_Z1_THRESHOLD, incomming_byte_count, incomming_bit_count);
    }
    else {
      Gpio::pin_map[miso_pin].value = 0;
    }
    if (current_state == XPT2046_Z1 && dirty && incomming_byte_count > 2) {
      current_state = 0;
    }
  }
  else if (ev.pin_id == clk_pin && ev.event == GpioEvent::FALL && Gpio::pin_map[cs_pin].value == 0) {
    incomming_byte = (incomming_byte << 1) | Gpio::pin_map[mosi_pin].value;
    if (++incomming_bit_count == 8) {
      incomming_bit_count = 0;
      incomming_byte_count++;
      if (incomming_byte == XPT2046_X) {
        // printf("Asking X   \n");
        current_state = XPT2046_X;
        incomming_byte_count = 0;
      }
      else if (incomming_byte == XPT2046_Y) {
        // printf("Asking Y\n");
        current_state = XPT2046_Y;
        incomming_byte_count = 0;
      }
      else if (incomming_byte == XPT2046_Z1) {
        // printf("Asking Z1\n");
        current_state = XPT2046_Z1;
        incomming_byte_count = 0;
      }
    }
  } else if (ev.pin_id == cs_pin && ev.event == GpioEvent::RISE) {
    //end data
    incomming_bit_count = incomming_byte_count = incomming_byte = 0;
    current_state = 0;
  }
}

void XPT2046Device::ui_callback(UiWindow* window) {
  if (ImGui::IsWindowFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    clickX = ImGui::GetIO().MousePos.x;
    clickY = ImGui::GetIO().MousePos.y;
    dirty = true;
  }
}

#endif
