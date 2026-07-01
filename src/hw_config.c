#include "g_config.h"
#include "hardware/gpio.h"
#include "hw_config.h"

const uint hw_gpio_pins[] = {
    HW_PIN_ROM_BANK_D0,
    HW_PIN_ROM_BANK_D1,
    HW_PIN_ROM_BANK_D2,
    HW_PIN_RAM_SIZE,
    HW_PIN_GOTEK_DRIVE_D0,
    HW_PIN_GOTEK_DRIVE_D1,
};

const int num_hw_gpio_pins = sizeof(hw_gpio_pins) / sizeof(hw_gpio_pins[0]);

void hw_init(hw_config_t *hw_config)
{
    for (size_t i = 0; i < num_hw_gpio_pins; i++)
    {
        gpio_init(hw_gpio_pins[i]);
        gpio_set_dir(hw_gpio_pins[i], GPIO_OUT);
    }

    // Set initial hardware configuration
    // Set ROM bank pins
    uint8_t rom_bank = hw_config->rom_bank - 1; // Convert to 0-based index
    gpio_put(HW_PIN_ROM_BANK_D0, rom_bank & 0x01);
    gpio_put(HW_PIN_ROM_BANK_D1, rom_bank & 0x02);
    gpio_put(HW_PIN_ROM_BANK_D2, rom_bank & 0x04);

    // Set RAM size pin
    gpio_put(HW_PIN_RAM_SIZE, hw_config->ram_size);

    // Set Gotek drive pins
    uint8_t gotek_drive = hw_config->gotek_drive;
    gpio_put(HW_PIN_GOTEK_DRIVE_D0, gotek_drive & 0x01);
    gpio_put(HW_PIN_GOTEK_DRIVE_D1, gotek_drive & 0x02);

#ifdef HW_GOTEK_BTN_L
    // Set Gotek button pins to HIGH (inactive state)
    gpio_init(HW_GOTEK_BTN_L);
    gpio_set_dir(HW_GOTEK_BTN_L, GPIO_IN);
    gpio_pull_up(HW_GOTEK_BTN_L);
#endif

#ifdef HW_GOTEK_BTN_R
    // Set Gotek button pins to HIGH (inactive state)
    gpio_init(HW_GOTEK_BTN_R);
    gpio_set_dir(HW_GOTEK_BTN_R, GPIO_IN);
    gpio_pull_up(HW_GOTEK_BTN_R);
#endif
}

void hw_set_ram_size(bool ram_size)
{
    gpio_put(HW_PIN_RAM_SIZE, ram_size);
}

void hw_set_rom_bank(uint8_t rom_bank)
{
    if (rom_bank < 1 || rom_bank > 8)
        return; // Invalid ROM bank, do nothing

    uint8_t rom_bank_index = rom_bank - 1; // Convert to 0-based index
    gpio_put(HW_PIN_ROM_BANK_D0, rom_bank_index & 0x01);
    gpio_put(HW_PIN_ROM_BANK_D1, rom_bank_index & 0x02);
    gpio_put(HW_PIN_ROM_BANK_D2, rom_bank_index & 0x04);
}

void hw_set_gotek_drive(uint8_t gotek_drive)
{
    if (gotek_drive > 2)
        return; // Invalid Gotek drive, do nothing

    gpio_put(HW_PIN_GOTEK_DRIVE_D0, gotek_drive & 0x01);
    gpio_put(HW_PIN_GOTEK_DRIVE_D1, gotek_drive & 0x02);
}