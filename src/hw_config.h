#pragma once

#include "g_config.h"

void hw_init(hw_config_t *hw_config);
void hw_set_ram_size(bool ram_size);
void hw_set_rom_bank(uint8_t rom_bank);
void hw_set_gotek_drive(uint8_t gotek_drive);