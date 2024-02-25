/*
 * Controller Buttons
 * WHowe <github.com/whowechina>
 * 
 */

#include "button.h"

#include <stdint.h>
#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"

#include "config.h"
#include "board_defs.h"

static const uint8_t button_gpio[] = BUTTON_DEF;

#define BUTTON_NUM (sizeof(button_gpio))

void button_init()
{
    for (int i = 0; i < BUTTON_NUM; i++)
    {
        int8_t gpio = button_gpio[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
    }
}

uint8_t button_num()
{
    return BUTTON_NUM;
}

bool button_pressed(int index)
{
    if (index >= BUTTON_NUM) {
        return false;
    }
    return !gpio_get(button_gpio[index]);
}
