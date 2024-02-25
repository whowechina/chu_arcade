/*
 * Chu Arcade Air Sensor
 * WHowe <github.com/whowechina>
 * 
 * Use ToF (Sharp GP2Y0E03) to detect air keys
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"

#include "board_defs.h"
#include "config.h"

#include "gp2y0e.h"
#include "vl53l0x.h"
#include "i2c_hub.h"

static const uint8_t TOF_LIST[] = TOF_MUX_LIST;
static uint8_t tof_model[sizeof(TOF_LIST)];
static uint16_t distances[sizeof(TOF_LIST) + 1]; // last one is always invalid

void air_init()
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_hub_init();
    gp2y0e_init(I2C_PORT);
    vl53l0x_init(I2C_PORT, 0);

    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        if (vl53l0x_is_present()) {
            tof_model[i] = 1;
        } else if (gp2y0e_is_present(I2C_PORT)) {
            tof_model[i] = 2;
        } else {
            tof_model[i] = 0;
        }
        if (tof_model[i] == 1) {
            vl53l0x_init_tof(i);
            vl53l0x_start_continuous(i);
        } else if (tof_model[i] == 2) {
            gp2y0e_init_sensor();
            gp2y0e_store(i);
        }
    }
}

size_t air_num()
{
    return sizeof(TOF_LIST);
}

static inline uint8_t air_bits(int dist)
{
    int offset= chu_cfg->tof.offset * 10;
    int pitch = chu_cfg->tof.pitch * 10;
    int bit1 = (dist - offset) / pitch;
    int bit2 = (dist - offset + pitch / 2) / pitch;
    
    return ((1 << bit1) | (1 << bit2)) & 0x3f;
}

static inline bool dist_valid(int dist)
{
    return (dist > chu_cfg->tof.offset * 10) &&
           (dist < chu_cfg->tof.offset * 10 + chu_cfg->tof.pitch * 10 * 6);
}

uint8_t air_bitmap()
{
    uint8_t bitmap = 0;
    int count = 0;
    int sum = 0;
    for (int i = 0; i < sizeof(TOF_LIST) + 1; i++) {
        if (dist_valid(distances[i])) {
            count++;
            sum += distances[i];
            continue;
        }
        if (count > 0) {
            uint16_t dist = sum / count;
            bitmap |= air_bits(dist);
            count = 0;
            sum = 0;
        }
    }

    return bitmap;
}

unsigned air_value(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }
    uint8_t bitmap = air_bits(distances[index]);

    for (int i = 0; i < 6; i++) {
        if (bitmap & (1 << i)) {
            return i + 1; // lowest detected position
        }
    }

    return 0;
}

void air_update()
{
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        if (tof_model[i] == 1) {
            distances[i] = readRangeContinuousMillimeters(i) * 10;
        } else if (tof_model[i] == 2) {
            distances[i] = gp2y0e_dist_mm(I2C_PORT) * 10;
        }
    }
}

uint16_t air_raw(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }
    return distances[index];
}
