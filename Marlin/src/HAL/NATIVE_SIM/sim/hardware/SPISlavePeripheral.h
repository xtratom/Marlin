#include "Gpio.h"

/**
 * Class to Easily Handle SPI Slave communication
 */
class SPISlavePeripheral : Peripheral {
public:
  SPISlavePeripheral(pin_type clk, pin_type miso, pin_type mosi, pin_type cs, uint8_t CPOL = 0, uint8_t CPHA = 0);
  virtual ~SPISlavePeripheral();

  // Callbacks
  virtual void onBeginTransaction();
  virtual void onEndTransaction();
  virtual void onBitReceived(uint8_t _bit);
  virtual void onBitSent(uint8_t _bit);
  virtual void onByteReceived(uint8_t _byte);
  virtual void onResponseSent();
  virtual void onByteSent(uint8_t _byte);

  void setResponse(uint8_t _data);
  void setResponse16(uint16_t _data, bool msb = true);
  void setResponse(uint8_t *_bytes, size_t count);

protected:
  void transmitCurrentBit();
  void spiInterrupt(GpioEvent& ev);

  pin_type clk_pin, miso_pin, mosi_pin, cs_pin;
  uint8_t CPOL, CPHA;

private:
  uint8_t incoming_byte = 0;
  uint8_t incoming_bit_count = 0;
  uint8_t incoming_byte_count = 0;
  uint8_t outgoing_byte = 0xFF;
  uint8_t outgoing_bit_count = 0;
  uint8_t *responseData = nullptr;
  size_t responseDataSize = 0;
  bool insideTransaction = false;
};
