#ifndef ARDUINO_I2C_SLAVE_H
#define ARDUINO_I2C_SLAVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*arduino_i2c_slave_rx_cb_t)(const uint8_t *data, size_t len);
    typedef size_t (*arduino_i2c_slave_tx_cb_t)(uint8_t *buffer, size_t max_len);

    void arduino_i2c_slave_begin(uint8_t sda_pin,
                                 uint8_t scl_pin,
                                 uint8_t slave_addr,
                                 arduino_i2c_slave_rx_cb_t rx_cb,
                                 arduino_i2c_slave_tx_cb_t tx_cb);

    void arduino_i2c_slave_set_address(uint8_t slave_addr);

#ifdef __cplusplus
}
#endif

#endif // ARDUINO_I2C_SLAVE_H
