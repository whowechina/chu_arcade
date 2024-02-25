/*
 * I2C Hub Using PCA9548A
 * WHowe <github.com/whowechina>
 */


#ifndef I2C_HUB_H
#define I2C_HUB_H

#include "hardware/i2c.h"
#include "board_defs.h"

#define I2C_HUB_ADDR_0 0x70
#define I2C_HUB_ADDR_1 0x71

static inline void i2c_hub_init()
{
}

static inline void i2c_select(i2c_inst_t *i2c_port, uint16_t chn)
{
    uint8_t addr0 = chn & 0xff;
    uint8_t addr1 = chn >> 8;
    i2c_write_blocking_until(i2c_port, I2C_HUB_ADDR_0, &addr0, 1, false, time_us_64() + 500);
    i2c_write_blocking_until(i2c_port, I2C_HUB_ADDR_1, &addr1, 1, false, time_us_64() + 500);
}

#endif
