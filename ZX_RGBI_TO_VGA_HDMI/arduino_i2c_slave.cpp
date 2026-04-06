#include <Arduino.h>
#include <Wire.h>

#include "hardware/irq.h"

#include "arduino_i2c_slave.h"

static arduino_i2c_slave_rx_cb_t g_rx_cb = nullptr;
static arduino_i2c_slave_tx_cb_t g_tx_cb = nullptr;
static TwoWire *g_wire = &Wire;
static bool g_use_i2c1 = false;

static void on_receive(int len);
static void on_request(void);

static void set_i2c_irq_priority(bool use_i2c1)
{
    // Keep I2C ISR above default-priority DMA ISR to avoid delayed STOP/RX handling.
    irq_set_priority(use_i2c1 ? I2C1_IRQ : I2C0_IRQ, PICO_HIGHEST_IRQ_PRIORITY);
}

static bool uses_i2c1_pins(uint8_t sda_pin, uint8_t scl_pin)
{
    // RP2040 legal I2C pin pairs are adjacent even/odd GPIOs.
    // Bases 2,6,10,14,18,22,26 map to I2C1 (Wire1).
    return (scl_pin == (uint8_t)(sda_pin + 1)) && ((sda_pin & 1u) == 0u) && ((sda_pin & 0x3u) == 0x2u);
}

static void configure_slave_bus(uint8_t sda_pin, uint8_t scl_pin, uint8_t slave_addr)
{
    Wire.end();
    Wire1.end();

    g_use_i2c1 = uses_i2c1_pins(sda_pin, scl_pin);
    g_wire = g_use_i2c1 ? &Wire1 : &Wire;

    g_wire->setSDA(sda_pin);
    g_wire->setSCL(scl_pin);
    g_wire->begin(slave_addr);
    g_wire->onReceive(on_receive);
    g_wire->onRequest(on_request);
    set_i2c_irq_priority(g_use_i2c1);
}

static void on_receive(int len)
{
    if ((g_rx_cb == nullptr) || (g_wire == nullptr) || (len <= 0))
        return;

    uint8_t buffer[256];
    size_t n = 0;

    while (g_wire->available() && (n < sizeof(buffer)))
        buffer[n++] = (uint8_t)g_wire->read();

    // Drain any extra bytes if master sent more than we can buffer.
    while (g_wire->available())
        (void)g_wire->read();

    g_rx_cb(buffer, n);
}

static void on_request(void)
{
    if ((g_tx_cb == nullptr) || (g_wire == nullptr))
        return;

    uint8_t buffer[32];
    size_t n = g_tx_cb(buffer, sizeof(buffer));

    if (n > 0)
        g_wire->write(buffer, n);
}

extern "C" void arduino_i2c_slave_begin(uint8_t sda_pin,
                                        uint8_t scl_pin,
                                        uint8_t slave_addr,
                                        arduino_i2c_slave_rx_cb_t rx_cb,
                                        arduino_i2c_slave_tx_cb_t tx_cb)
{
    g_rx_cb = rx_cb;
    g_tx_cb = tx_cb;

    configure_slave_bus(sda_pin, scl_pin, slave_addr);
}

extern "C" void arduino_i2c_slave_set_address(uint8_t slave_addr)
{
    if (g_wire == nullptr)
    {
        g_wire = &Wire;
        g_use_i2c1 = false;
    }

    g_wire->end();
    g_wire->begin(slave_addr);
    g_wire->onReceive(on_receive);
    g_wire->onRequest(on_request);
    set_i2c_irq_priority(g_use_i2c1);
}
