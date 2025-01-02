/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/clocks.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "board_defs.h"

#include "save.h"
#include "config.h"
#include "cli.h"
#include "commands.h"

#include "air.h"
#include "rgb.h"
#include "button.h"

struct __attribute__((packed)) {
    uint16_t adcs[8];
    uint16_t spinners[4];
    uint16_t chutes[2];
    uint16_t buttons[2];
    uint8_t system_status;
    uint8_t usb_status;
    uint8_t padding[29];
} hid_joy;

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro, sent_hid_nkro;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        if (chu_cfg->hid.joy) {
            tud_hid_n_report(0, REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy));
        }
        if (chu_cfg->hid.nkro &&
            (memcmp(&hid_nkro, &sent_hid_nkro, sizeof(hid_nkro)) != 0)) {
            sent_hid_nkro = hid_nkro;
            tud_hid_n_report(1, 0, &sent_hid_nkro, sizeof(sent_hid_nkro));
        }
    }
}

static void gen_joy_report()
{
    hid_joy.buttons[0] = 0;
    hid_joy.buttons[1] = 0;

    if (button_pressed(0)) {
        hid_joy.buttons[0] |= 1 << 9;
    }
    if (button_pressed(1)) {
        hid_joy.buttons[0] |= 1 << 6;
    }

    static bool last_coin_pressed = false;
    bool coin_pressed = button_pressed(2);
    if (coin_pressed && !last_coin_pressed) {
        hid_joy.chutes[0] += 0x100;
    }
    last_coin_pressed = coin_pressed;

    uint16_t air[6][2] = {
        { 1 << 13, 0 },
        { 0, 1 << 13 },
        { 1 << 12, 0 },
        { 0, 1 << 12 },
        { 1 << 11, 0 },
        { 0, 1 << 11 },
    };

    uint8_t airkey = air_bitmap();
    for (int i = 0; i < 6; i++) {
        if (~airkey & (1 << i)) {
            hid_joy.buttons[0] |= air[i][0];
            hid_joy.buttons[1] |= air[i][1];
        }
    }
}

const uint8_t keycode_table[128][2] = { HID_ASCII_TO_KEYCODE };
const uint8_t keymap[38 + 1] = NKRO_KEYMAP; // 32 keys, 6 air keys, 1 terminator
static void gen_nkro_report()
{
    uint8_t air = air_bitmap();
    for (int i = 0; i < 6; i++) {
        uint8_t code = keycode_table[keymap[32 + i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (air & (1 << i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }
}

static uint64_t last_hid_time = 0;

static void run_lights()
{
    uint64_t now = time_us_64();

    if (now - last_hid_time >= 1000000) {
        const uint32_t colors[] = {0x080808, 0x000080, 0x800000, 0x008000,
                                0x008080, 0x808000, 0x800080};
        for (int i = 0; i < air_num(); i++) {
            int d = air_value(i);
            rgb_set_color(i, colors[d]);
        }
    }
}

void bridge_slider_port()
{
    if (tud_cdc_n_available(1)) {
        uint8_t buf[128];
        int len = tud_cdc_n_read(1, buf, sizeof(buf));
        if (len > 0) {
            uart_write_blocking(SLIDER_UART, buf, len);
        }
    }
    while (uart_is_readable(SLIDER_UART)) {
        tud_cdc_n_write_char(1, uart_getc(SLIDER_UART));
        tud_cdc_n_write_flush(1);
    }
}

static mutex_t core1_io_lock;
static void core1_loop()
{
    while (1) {
        if (mutex_try_enter(&core1_io_lock, NULL)) {
            run_lights();
            rgb_update();
            mutex_exit(&core1_io_lock);
        }
        cli_fps_count(1);
        bridge_slider_port();
        sleep_us(100);
    }
}

static void core0_loop()
{
    while(1) {
        tud_task();

        cli_run();
        //aime_update();
    
        save_loop();
        cli_fps_count(0);

        air_update();

        gen_joy_report();
        gen_nkro_report();
        report_usb_hid();
    }
}

void slider_init()
{
    // init uart for slider port
    gpio_set_function(SLIDER_TX, GPIO_FUNC_UART);
    gpio_set_function(SLIDER_RX, GPIO_FUNC_UART);
    gpio_pull_up(SLIDER_TX);
    gpio_pull_up(SLIDER_RX);
    uart_init(SLIDER_UART, 115200);
}

void update_check()
{
    uint8_t pins[] = BUTTON_DEF;

    for (int i = 0; i < sizeof(pins); i++) {
        uint8_t gpio = pins[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        sleep_ms(1);
        if (button_pressed(i)) {
            sleep_ms(100);
            reset_usb_boot(0, 2);
        }
    }
}

void init()
{
    sleep_ms(50);
    set_sys_clock_khz(180000, true);
    board_init();
    update_check();

    tusb_init();
    stdio_init_all();

    config_init();
    mutex_init(&core1_io_lock);
    save_init(0xca341234, &core1_io_lock);

    air_init();
    rgb_init();
    button_init();
    slider_init();

    cli_init("chu_arcade>", "\n   << Chu Arcade Controller >>\n"
                            " https://github.com/whowechina\n\n");

    commands_init();
}

int main(void)
{
    init();
    multicore_launch_core1(core1_loop);
    core0_loop();
    return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    printf("Get from USB %d-%d\n", report_id, report_type);
    return 0;
}

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t cmd;
    uint8_t payload[62];
} hid_output_t;

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    hid_output_t *output = (hid_output_t *)buffer;
    if (output->report_id != REPORT_ID_OUTPUT) {
        return;
    }

    switch (output->cmd) {
        case 0x01:
        case 0x02:
            hid_joy.system_status = 0x30;
            break;
        case 0x03:
            hid_joy.chutes[0] = 0;
            hid_joy.chutes[1] = 0;
            hid_joy.system_status = 0;
            printf("IO4 reset.\n");
            break;
        case 0x04:
        case 0x41:
            break;
        default:
            printf("IO4 unknown cmd: %02x\n", output->cmd);
            break;
        break;
    }
}
