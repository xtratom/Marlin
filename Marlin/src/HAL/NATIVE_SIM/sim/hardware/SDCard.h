#pragma once

#include "../user_interface.h"

#include "SPISlavePeripheral.h"

class SDCard: public SPISlavePeripheral {
public:
  SDCard(pin_type clk, pin_type mosi, pin_type miso, pin_type cs, pin_type sd_detect = -1) : SPISlavePeripheral(clk, mosi, miso, cs), sd_detect(sd_detect) {}
  virtual ~SDCard() {};

  pin_type sd_detect;

  void update() {}
  void ui_callback(UiWindow* window);

  void onByteReceived(uint8_t _byte) override;
  void onEndTransaction() override {
    SPISlavePeripheral::onEndTransaction();
  };
  void onResponseSent() override;

  int8_t currentCommand = -1;
  int32_t arg = 0;
  int32_t byteCount = 0;
  uint8_t crc = 0;
  uint8_t buf[1024];
  FILE *fp = nullptr;
};
