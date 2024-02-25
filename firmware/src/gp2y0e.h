/*
 * GP2Y0E Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef GP2Y0E_H
#define GP2Y0E_H

#include <stdint.h>
#include "hardware/i2c.h"

#define GP2Y0E_DEF_ADDR 0x40

static i2c_inst_t *gp2y0e_port = i2c0;

static inline void gp2y0e_init(i2c_inst_t *port)
{
    gp2y0e_port = port;
}

static inline void gp2y0e_write(uint8_t addr, uint8_t val)
{
    uint8_t cmd[] = {addr, val};
    i2c_write_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, cmd, 2,
                             false, time_us_64() + 1000);
}

static inline void gp2y0e_init_sensor()
{
    gp2y0e_write(0xa8, 0); // Accumulation 0:1, 1:5, 2:30, 3:10
    gp2y0e_write(0x3f, 0x30); // Filter 0x00:7, 0x10:5, 0x20:9, 0x30:1
    gp2y0e_write(0x13, 6); // Pulse [3..7]:[40, 80, 160, 240, 320] us
    gp2y0e_write(0x1c, 12); // [4:0]
    gp2y0e_write(0x2f, 170); // [6:0]
    gp2y0e_write(0x33, 110); // [7:0]
    gp2y0e_write(0x34, 50); // [6:0]
}

static uint8_t gp2y0e_read(uint8_t reg)
{
    i2c_write_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, &reg, 1,
                             true, time_us_64() + 1000);
    uint8_t data;
    i2c_read_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, &data, 1,
                            false, time_us_64() + 1000);
    return data;
}

uint8_t gp2y0e_regs[12][4];
static void gp2y0e_store(uint8_t index)
{
    gp2y0e_regs[index][0] = gp2y0e_read(0x1c);
    gp2y0e_regs[index][1] = gp2y0e_read(0x2f);
    gp2y0e_regs[index][2] = gp2y0e_read(0x33);
    gp2y0e_regs[index][3] = gp2y0e_read(0x34);    
}

static inline bool gp2y0e_is_present()
{
    uint8_t cmd[] = {0x5e};
    int bytes = i2c_write_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, cmd, 1,
                                          true, time_us_64() + 1000);
    return bytes == 1;
}

static inline uint16_t gp2y0e_dist_mm()
{
    uint8_t cmd[] = {0x5e};
    i2c_write_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, cmd, 1,
                             true, time_us_64() + 1000);
    uint8_t data;
    i2c_read_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, &data, 1,
                             false, time_us_64() + 1000);

    return data * 10 / 4;
}

static inline uint16_t gp2y0e_dist16_mm()
{
    uint8_t cmd[] = {0x5e};
    i2c_write_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, cmd, 1,
                             true, time_us_64() + 1000);
    uint8_t data[2];
    i2c_read_blocking_until(gp2y0e_port, GP2Y0E_DEF_ADDR, data, 2,
                             false, time_us_64() + 1000);

    return ((data[0] << 4) | (data[1] & 0x0f)) * 10 / 64;
}

#endif