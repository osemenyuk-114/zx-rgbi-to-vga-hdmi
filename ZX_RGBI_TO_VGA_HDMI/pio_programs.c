#include "g_config.h"
#include "pio_programs.h"

const uint16_t nop_opcode = 0xa042;

// self-synchronizing capture PIO program
uint16_t pio_program_capture_0_instructions[] = {
    //             .wrap_target
    0xa042, //  0: nop               // the capture delay will be added to this command
    0x4008, //  1: in     pins, 8
    0x8020, //  2: push   block
    0xa842, //  3: nop    [8]
    0x00c1, //  4: jmp    pin, 1
    0x4008, //  5: in     pins, 8    // sub-synchronization by sync pulse
    0x8020, //  6: push   block
    0x00c0, //  7: jmp    pin, 0
    0x00c0, //  8: jmp    pin, 0
    0x00c0, //  9: jmp    pin, 0
    0x00c0, // 10: jmp    pin, 0
    0x00c0, // 11: jmp    pin, 0
    0x00c0, // 12: jmp    pin, 0
    0x00c0, // 13: jmp    pin, 0
    0x00c0, // 14: jmp    pin, 0
    0x00c0, // 15: jmp    pin, 0
    0x0005, // 16: jmp    5
            //     .wrap
};

const struct pio_program pio_program_capture_0 = {
    .instructions = pio_program_capture_0_instructions,
    .length = 17,
    .origin = -1,
};

const uint16_t set_opcode = 0xe020;

// PIO program with an external capture clock source
uint16_t pio_program_capture_1_instructions[] = {
    //                         .wrap_target
    0xa042,         //  0: nop                 // a capture delay will be added to this command
    0xe020,         //  1: set    x, 0         // an external clock divider will be added to this command
    0x2000 | F_PIN, //  2: wait   1 gpio, 0
    0x4008,         //  3: in     pins, 8
    0x2080 | F_PIN, //  4: wait   0 gpio, 0
    0x0042,         //  5: jmp    x--, 2
    0x8020,         //  6: push   block
    0x00c1,         //  7: jmp    pin, 1
    0xe020,         //  8: set    x, 0         // an external clock divider will be added to this command
    0x2000 | F_PIN, //  9: wait   1 gpio, 0
    0x00c0,         // 10: jmp    pin, 0
    0x4008,         // 11: in     pins, 8
    0x2080 | F_PIN, // 12: wait   0 gpio, 1
    0x0049,         // 13: jmp    x--, 9
    0x8020,         // 14: push   block
    0x0008,         // 15: jmp    8
                    //     .wrap

};

const struct pio_program pio_program_capture_1 = {
    .instructions = pio_program_capture_1_instructions,
    .length = 16,
    .origin = -1,
};

uint16_t pio_program_vga_instructions[] = {
    //             .wrap_target
    0x6008, //  0: out    pins, 8
            //     .wrap
};

const struct pio_program pio_program_vga = {
    .instructions = pio_program_vga_instructions,
    .length = 1,
    .origin = -1,
};

uint16_t pio_program_dvi_instructions[] = {
    //             .wrap_target
    0x7006, //  0: out pins, 6   side 2
    0x7006, //  1: out pins, 6   side 2
    0x7006, //  2: out pins, 6   side 2
    0x7006, //  3: out pins, 6   side 2
    0x7006, //  4: out pins, 6   side 2
    0x6806, //  5: out pins, 6   side 1
    0x6806, //  6: out pins, 6   side 1
    0x6806, //  7: out pins, 6   side 1
    0x6806, //  8: out pins, 6   side 1
    0x6806, //  9: out pins, 6   side 1
            //     .wrap
};

const struct pio_program pio_program_dvi = {
    .instructions = pio_program_dvi_instructions,
    .length = 10,
    .origin = -1,
};