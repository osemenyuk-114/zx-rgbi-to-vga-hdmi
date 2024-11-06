#ifndef PIO_PROGRAMS_H
#define PIO_PROGRAMS_H

#include "hardware/pio.h"

extern const uint16_t nop_opcode;

extern const struct pio_program pio_program_capture_0;
extern uint16_t pio_program_capture_0_instructions[];

extern const uint16_t set_opcode;

extern const struct pio_program pio_program_capture_1;
extern uint16_t pio_program_capture_1_instructions[];
/*
extern const struct pio_program pio_program_capture_2;
extern uint16_t pio_program_capture_2_instructions[];
*/

extern const struct pio_program pio_program_vga;
extern uint16_t pio_program_vga_instructions[];

extern const struct pio_program pio_program_hdmi;
extern uint16_t pio_program_hdmi_instructions[];

#endif