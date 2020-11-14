/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../inc/MarlinConfigPre.h"

/**
 * Adjust USB_DEBUG to select debugging verbosity.
 *    0 - no debug messages
 *    1 - basic insertion/removal messages
 *    2 - show USB state transitions
 *    3 - perform block range checking
 *    4 - print each block access
 */
#define USB_DEBUG         1
#define USB_STARTUP_DELAY 0

// uncomment to get 'printf' console debugging. NOT FOR UNO!
//#define HOST_DEBUG(...)     {char s[255]; sprintf(s,__VA_ARGS__); SERIAL_ECHOLNPAIR("UHS:",s);}
//#define BS_HOST_DEBUG(...)  {char s[255]; sprintf(s,__VA_ARGS__); SERIAL_ECHOLNPAIR("UHS:",s);}
//#define MAX_HOST_DEBUG(...) {char s[255]; sprintf(s,__VA_ARGS__); SERIAL_ECHOLNPAIR("UHS:",s);}

#if ENABLED(USB_FLASH_DRIVE_SUPPORT)

#include "../../MarlinCore.h"
#include "../../core/serial.h"
#include "../../module/temperature.h"

static enum {
  UNINITIALIZED,
  DO_STARTUP,
  WAIT_FOR_DEVICE,
  WAIT_FOR_LUN,
  MEDIA_READY,
  MEDIA_ERROR
} state;

#if defined(USE_USB_HOST)
  #define USB_OTG_FS_DP 1
  #define USB_OTG_FS_DM 1
#else
  static_assert(USB_CS_PIN   != -1, "USB_CS_PIN must be defined");
  static_assert(USB_INTR_PIN != -1, "USB_INTR_PIN must be defined");
#endif

#if ENABLED(USE_UHS3_USB)
  #define NO_AUTO_SPEED
  #define UHS_MAX3421E_SPD 8000000 >> SPI_SPEED
  #define UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE 1
  #define UHS_HOST_MAX_INTERFACE_DRIVERS 2
  #define MASS_MAX_SUPPORTED_LUN 1
  #define USB_HOST_SERIAL MYSERIAL0

  // Workaround for certain issues with UHS3
  #define SKIP_PAGE3F // Required for IOGEAR media adapter
  #define USB_NO_TEST_UNIT_READY // Required for removable media adapter
  #define USB_HOST_MANUAL_POLL // Optimization to shut off IRQ automatically

  // Workarounds for keeping Marlin's watchdog timer from barking...
  void marlin_yield() {
    thermalManager.manage_heater();
  }
  #define SYSTEM_OR_SPECIAL_YIELD(...) marlin_yield();
  #define delay(x) safe_delay(x)

  #define LOAD_USB_HOST_SYSTEM
  #define LOAD_USB_HOST_SHIELD
  #define LOAD_UHS_BULK_STORAGE

  #define MARLIN_UHS_WRITE_SS(v) WRITE(USB_CS_PIN, v)
  #define MARLIN_UHS_READ_IRQ()  READ(USB_INTR_PIN)

  #include "lib-uhs3/UHS_host/UHS_host.h"

  MAX3421E_HOST usb(USB_CS_PIN, USB_INTR_PIN);
  UHS_Bulk_Storage bulk(&usb);

  #define UHS_START  (usb.Init() == 0)
  #define UHS_STATE(state) UHS_USB_HOST_STATE_##state
#elif defined(USE_USB_HOST)
  #include "usbh_core.h"
  #include "usbh_msc.h"

  typedef enum {
    USB_STATE_INIT,
    USB_STATE_ERROR,
    USB_STATE_RUNNING,
  } usb_state_t;

  // /*
  // * Background task
  // */
  // void MX_USB_HOST_Process(void)
  // {
  //   /* USB Host Background task */
  //   USBH_Process(&hUsbHostFS);
  // }
  /*
  * user callback definition
  */
 static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id);

  class USB {
  public:
    USBH_HandleTypeDef hUsbHostFS;

    bool start() {
      // USBH_Init(&usbh, NULL, 1);
      if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
      {
        SERIAL_ECHOLN("Error: USBH_Init");
      }
      if (USBH_RegisterClass(&hUsbHostFS, USBH_MSC_CLASS) != USBH_OK)
      {
        SERIAL_ECHOLN("Error: USBH_Init");
      }
      if (USBH_Start(&hUsbHostFS) != USBH_OK)
      {
        SERIAL_ECHOLN("Error: USBH_Start");
      }
      return true;
    }

    void Task() {
      USBH_Process(&hUsbHostFS);
    }

    uint8_t getUsbTaskState() {
      return usb_task_state;
    }

    void setUsbTaskState(uint8_t state) {
      usb_task_state = state;
    }

    // uint8_t getUsbTaskState() {
    //   ret
    // }

    uint8_t regRd(uint8_t reg) {
      return 0x0;
    }

    uint8_t usb_task_state = USB_STATE_INIT;
    uint8_t lun = 0;
  };

  class Bulk {
  public:
    Bulk(USB *usb) : usb(usb) {}
    bool LUNIsGood(uint8_t t) { return USBH_MSC_IsReady(&usb->hUsbHostFS) && USBH_MSC_UnitIsReady(&usb->hUsbHostFS, t); }
    uint32_t GetCapacity(uint8_t lun) { return 15669247; }
    uint16_t GetSectorSize(uint8_t lun) { return 512; }
    uint8_t Read(uint8_t lun, uint32_t addr, uint16_t bsize, uint8_t blocks, uint8_t *buf) {
      // memset(buf, 0, 512);
      USBH_StatusTypeDef ret = USBH_MSC_Read(&usb->hUsbHostFS, lun, addr, buf, blocks);
      // SERIAL_ECHOLNPAIR("Ret: ", ret);
      return 0;
    }
    uint8_t Write(uint8_t lun, uint32_t addr, uint16_t bsize, uint8_t blocks, const uint8_t * buf) {
      return USBH_MSC_Write(&usb->hUsbHostFS, lun, addr, const_cast <uint8_t*>(buf), bsize * blocks) == USBH_OK;
    }

    USB *usb;
  };

  USB usb;
  Bulk bulk(&usb);
  #define UHS_START usb.start()
  #define rREVISION 0
  #define UHS_STATE(state) USB_STATE_##state

  static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
  {
    /* USER CODE BEGIN CALL_BACK_1 */
    switch(id)
    {
    case HOST_USER_SELECT_CONFIGURATION:
    SERIAL_ECHOLN("APPLICATION_SELECT_CONFIGURATION");
    break;

    case HOST_USER_DISCONNECTION:
    // Appli_state = APPLICATION_DISCONNECT;
      SERIAL_ECHOLN("APPLICATION_DISCONNECT");
      // usb.setUsbTaskState(USB_STATE_RUNNING);
    break;

    case HOST_USER_CLASS_ACTIVE:
    // Appli_state = APPLICATION_READY;
      {
      SERIAL_ECHOLN("APPLICATION_READY");
      MSC_LUNTypeDef info;
      USBH_MSC_GetLUNInfo(phost, id, &info);
      uint32_t cap = info.capacity.block_nbr / 2000;
      SERIAL_ECHOLNPAIR("info.capacity.block_nbr : %ld\n", info.capacity.block_nbr);
      SERIAL_ECHOLNPAIR("info.capacity.block_size: %d\n", info.capacity.block_size);
      SERIAL_ECHOLNPAIR("capacity                : %d MB\n", cap);
      usb.setUsbTaskState(USB_STATE_RUNNING);
      // usb.lun = id;
      }
    break;

    case HOST_USER_CONNECTION:
    // Appli_state = APPLICATION_START;
      SERIAL_ECHOLN("APPLICATION_START");
    break;

    default:
    break;
    }
    /* USER CODE END CALL_BACK_1 */
  }

#else
  #include "lib-uhs2/Usb.h"
  #include "lib-uhs2/masstorage.h"

  USB usb;
  BulkOnly bulk(&usb);

  #define UHS_START usb.start()
  #define UHS_STATE(state) USB_STATE_##state
#endif

#include "Sd2Card_FlashDrive.h"

#include "../../lcd/marlinui.h"

#if USB_DEBUG >= 3
  uint32_t lun0_capacity;
#endif

bool Sd2Card::usbStartup() {
  if (state <= DO_STARTUP) {
    SERIAL_ECHOPGM("Starting USB host...");
    if (!UHS_START) {
      SERIAL_ECHOLNPGM(" failed.");
      LCD_MESSAGEPGM(MSG_MEDIA_USB_FAILED);
      return false;
    }

    // SPI quick test - check revision register
    switch (usb.regRd(rREVISION)) {
      case 0x01: SERIAL_ECHOLNPGM("rev.01 started"); break;
      case 0x12: SERIAL_ECHOLNPGM("rev.02 started"); break;
      case 0x13: SERIAL_ECHOLNPGM("rev.03 started"); break;
      default:   SERIAL_ECHOLNPGM("started. rev unknown."); break;
    }
    state = WAIT_FOR_DEVICE;
  }
  return true;
}

// The USB library needs to be called periodically to detect USB thumbdrive
// insertion and removals. Call this idle() function periodically to allow
// the USB library to monitor for such events. This function also takes care
// of initializing the USB library for the first time.

void Sd2Card::idle() {
  usb.Task();

  const uint8_t task_state = usb.getUsbTaskState();

  #if USB_DEBUG >= 2
    if (state > DO_STARTUP) {
      static uint8_t laststate = 232;
      if (task_state != laststate) {
        laststate = task_state;
        #define UHS_USB_DEBUG(x) case UHS_STATE(x): SERIAL_ECHOLNPGM(#x); break
        switch (task_state) {
          UHS_USB_DEBUG(IDLE);
          UHS_USB_DEBUG(RESET_DEVICE);
          UHS_USB_DEBUG(RESET_NOT_COMPLETE);
          UHS_USB_DEBUG(DEBOUNCE);
          UHS_USB_DEBUG(DEBOUNCE_NOT_COMPLETE);
          UHS_USB_DEBUG(WAIT_SOF);
          UHS_USB_DEBUG(ERROR);
          UHS_USB_DEBUG(CONFIGURING);
          UHS_USB_DEBUG(CONFIGURING_DONE);
          UHS_USB_DEBUG(RUNNING);
          default:
            SERIAL_ECHOLNPAIR("UHS_USB_HOST_STATE: ", task_state);
            break;
        }
      }
    }
  #endif

  static millis_t next_state_ms = millis();

  #define GOTO_STATE_AFTER_DELAY(STATE, DELAY) do{ state = STATE; next_state_ms  = millis() + DELAY; }while(0)

  if (ELAPSED(millis(), next_state_ms)) {
    GOTO_STATE_AFTER_DELAY(state, 250); // Default delay

    switch (state) {

      case UNINITIALIZED:
        #ifndef MANUAL_USB_STARTUP
          GOTO_STATE_AFTER_DELAY( DO_STARTUP, USB_STARTUP_DELAY );
        #endif
        break;

      case DO_STARTUP: usbStartup(); break;

      case WAIT_FOR_DEVICE:
        if (task_state == UHS_STATE(RUNNING)) {
          #if USB_DEBUG >= 1
            SERIAL_ECHOLNPGM("USB device inserted");
          #endif
          GOTO_STATE_AFTER_DELAY( WAIT_FOR_LUN, 250 );
        }
        break;

      case WAIT_FOR_LUN:
        /* USB device is inserted, but if it is an SD card,
         * adapter it may not have an SD card in it yet. */
        if (bulk.LUNIsGood(0)) {
          #if USB_DEBUG >= 1
            SERIAL_ECHOLNPGM("LUN is good");
          #endif
          GOTO_STATE_AFTER_DELAY( MEDIA_READY, 100 );
        }
        else {
          #ifdef USB_HOST_MANUAL_POLL
            // Make sure we catch disconnect events
            usb.busprobe();
            usb.VBUS_changed();
          #endif
          #if USB_DEBUG >= 1
            SERIAL_ECHOLNPGM("Waiting for media");
          #endif
          LCD_MESSAGEPGM(MSG_MEDIA_WAITING);
          GOTO_STATE_AFTER_DELAY(state, 2000);
        }
        break;

      case MEDIA_READY: break;
      case MEDIA_ERROR: break;
    }

    if (state > WAIT_FOR_DEVICE && task_state != UHS_STATE(RUNNING)) {
      // Handle device removal events
      #if USB_DEBUG >= 1
        SERIAL_ECHOLNPGM("USB device removed");
      #endif
      if (state != MEDIA_READY)
        LCD_MESSAGEPGM(MSG_MEDIA_USB_REMOVED);
      GOTO_STATE_AFTER_DELAY(WAIT_FOR_DEVICE, 0);
    }

    else if (state > WAIT_FOR_LUN && !bulk.LUNIsGood(0)) {
      // Handle media removal events
      #if USB_DEBUG >= 1
        SERIAL_ECHOLNPGM("Media removed");
      #endif
      LCD_MESSAGEPGM(MSG_MEDIA_REMOVED);
      GOTO_STATE_AFTER_DELAY(WAIT_FOR_DEVICE, 0);
    }

    else if (task_state == UHS_STATE(ERROR)) {
      LCD_MESSAGEPGM(MSG_MEDIA_READ_ERROR);
      GOTO_STATE_AFTER_DELAY(MEDIA_ERROR, 0);
    }
  }
}

// Marlin calls this function to check whether an USB drive is inserted.
// This is equivalent to polling the SD_DETECT when using SD cards.
bool Sd2Card::isInserted() {
  return state == MEDIA_READY;
}

bool Sd2Card::ready() {
  return state > DO_STARTUP;
}

// Marlin calls this to initialize an SD card once it is inserted.
bool Sd2Card::init(const uint8_t, const pin_t) {
  if (!isInserted()) return false;

  #if USB_DEBUG >= 1
  const uint32_t sectorSize = bulk.GetSectorSize(0);
  if (sectorSize != 512) {
    SERIAL_ECHOLNPAIR("Expecting sector size of 512. Got: ", sectorSize);
    return false;
  }
  #endif

  #if USB_DEBUG >= 3
    lun0_capacity = bulk.GetCapacity(0);
    SERIAL_ECHOLNPAIR("LUN Capacity (in blocks): ", lun0_capacity);
  #endif
  return true;
}

// Returns the capacity of the card in blocks.
uint32_t Sd2Card::cardSize() {
  if (!isInserted()) return false;
  #if USB_DEBUG < 3
    const uint32_t
  #endif
      lun0_capacity = bulk.GetCapacity(0);
  return lun0_capacity;
}

bool Sd2Card::readBlock(uint32_t block, uint8_t* dst) {
  if (!isInserted()) return false;
  #if USB_DEBUG >= 3
    if (block >= lun0_capacity) {
      SERIAL_ECHOLNPAIR("Attempt to read past end of LUN: ", block);
      return false;
    }
    #if USB_DEBUG >= 4
      SERIAL_ECHOLNPAIR("Read block ", block);
    #endif
  #endif
  return bulk.Read(0, block, 512, 1, dst) == 0;
}

bool Sd2Card::writeBlock(uint32_t block, const uint8_t* src) {
  if (!isInserted()) return false;
  #if USB_DEBUG >= 3
    if (block >= lun0_capacity) {
      SERIAL_ECHOLNPAIR("Attempt to write past end of LUN: ", block);
      return false;
    }
    #if USB_DEBUG >= 4
      SERIAL_ECHOLNPAIR("Write block ", block);
    #endif
  #endif
  return bulk.Write(0, block, 512, 1, src) == 0;
}

#endif // USB_FLASH_DRIVE_SUPPORT
