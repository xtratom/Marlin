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
#pragma once

#include "../inc/MarlinConfigPre.h"

/**
 * Instructions for create a FAT image:
 * 1) Install mtools
 * 2) Create the imagem file:
 *    $ mformat -v "EMBEDDED FS" -t 1 -h 1 -s 10000 -S 2 -C -i fs.img -c 1 -r 1 -L 1
 *    -s NUM is the number of sectors
 * 3) Copy files to the image:
 *    $ mcopy -i fs.img CFFFP_flow_calibrator.gcode ::/
 * 4) Set the path for SD_SIMULATOR_FAT_IMAGE
 */
//#define SD_SIMULATOR_FAT_IMAGE "/full/path/to/fs.img"
#ifndef SD_SIMULATOR_FAT_IMAGE
  #error "You need set SD_SIMULATOR_FAT_IMAGE with a path for a FAT filesystem image."
#endif

class Sd2Card {
  private:
    FILE *fp;
  public:
    bool init(uint8_t sckRateID = 0, uint8_t chipSelectPin = 0) {
      fp = fopen(SD_SIMULATOR_FAT_IMAGE, "rb");
      return true;
    }

    bool readBlock(uint32_t block, uint8_t *dst) {
      fseek(fp, 512 * block, SEEK_SET);
      fread(dst, 512, 1, fp);
      return true;
    }

    bool writeBlock(uint32_t block, const uint8_t *src) {
      return true;
    }
};
