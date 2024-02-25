#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "tusb.h"

#include "config.h"
#include "air.h"
#include "save.h"
#include "cli.h"

#include "i2c_hub.h"

#include "pn532.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_colors()
{
    printf("[Colors]\n");
    printf("  Key upper: %06lx, lower: %06lx, both: %06lx, off: %06lx\n", 
           chu_cfg->colors.key_on_upper, chu_cfg->colors.key_on_lower,
           chu_cfg->colors.key_on_both, chu_cfg->colors.key_off);
    printf("  Gap: %06lx\n", chu_cfg->colors.gap);
}

static void disp_style()
{
    printf("[Style]\n");
    printf("  Key: %d, Gap: %d, ToF: %d, Level: %d\n",
           chu_cfg->style.key, chu_cfg->style.gap,
           chu_cfg->style.tof, chu_cfg->style.level);
}

static void disp_tof()
{
    printf("[ToF]\n");
    printf("  Offset: %d, Pitch: %d\n", chu_cfg->tof.offset, chu_cfg->tof.pitch);
}

static void disp_sense()
{
    printf("[Sense]\n");
    printf("  Filter: %u, %u, %u\n", chu_cfg->sense.filter >> 6,
                                    (chu_cfg->sense.filter >> 4) & 0x03,
                                    chu_cfg->sense.filter & 0x07);
    printf("  Sensitivity (global: %+d):\n", chu_cfg->sense.global);
    printf("    | 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|\n");
    printf("  ---------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2]);
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2 + 1]);
    }
    printf("\n");
    printf("  Debounce (touch, release): %d, %d\n",
           chu_cfg->sense.debounce_touch, chu_cfg->sense.debounce_release);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           chu_cfg->hid.joy ? "on" : "off",
           chu_cfg->hid.nkro ? "on" : "off" );
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [colors|style|tof|sense|hid]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_colors();
        disp_style();
        disp_tof();
        disp_sense();
        disp_hid();
        return;
    }

    const char *choices[] = {"colors", "style", "tof", "sense", "hid"};
    switch (cli_match_prefix(choices, 5, argv[0])) {
        case 0:
            disp_colors();
            break;
        case 1:
            disp_style();
            break;
        case 2:
            disp_tof();
            break;
        case 3:
            disp_sense();
            break;
        case 4:
            disp_hid();
            break;
        default:
            printf(usage);
            break;
    }
}

static int fps[2];
void fps_count(int core)
{
    static uint32_t last[2] = {0};
    static int counter[2] = {0};

    counter[core]++;

    uint32_t now = time_us_32();
    if (now - last[core] < 1000000) {
        return;
    }
    last[core] = now;
    fps[core] = counter[core];
    counter[core] = 0;
}

static void handle_level(int argc, char *argv[])
{
    const char *usage = "Usage: level <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int level = cli_extract_non_neg_int(argv[0], 0);
    if ((level < 0) || (level > 255)) {
        printf(usage);
        return;
    }

    chu_cfg->style.level = level;
    config_changed();
    disp_style();
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <joy|nkro|both>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"joy", "nkro", "both"};
    int match = cli_match_prefix(choices, 3, argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    chu_cfg->hid.joy = ((match == 0) || (match == 2)) ? 1 : 0;
    chu_cfg->hid.nkro = ((match == 1) || (match == 2)) ? 1 : 0;
    config_changed();
    disp_hid();
}

extern uint8_t gp2y0e_regs[12][4];

static void handle_tof(int argc, char *argv[])
{
    const char *usage = "Usage: tof <offset> [pitch]\n"
                        "  offset: 40..255\n"
                        "  pitch: 4..50\n";
    
    for (int i = 0; i < 12; i++) {
        printf("%2d: %3d %3d %3d %3d\n", i,
        gp2y0e_regs[i][0], gp2y0e_regs[i][1],
        gp2y0e_regs[i][2], gp2y0e_regs[i][3]);
    }
    if (argc > 2) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        printf("TOF: ");
        for (int i = air_num(); i > 0; i--) {
            printf(" %4d", air_raw(i - 1) / 10);
        }
        printf("\n");
        return;
    }

    int offset = chu_cfg->tof.offset;
    int pitch = chu_cfg->tof.pitch;
    if (argc >= 1) {
        offset = cli_extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        pitch = cli_extract_non_neg_int(argv[1], 0);
    }

    if ((offset < 40) || (offset > 255) || (pitch < 4) || (pitch > 50)) {
        printf(usage);
        return;
    }

    chu_cfg->tof.offset = offset;
    chu_cfg->tof.pitch = pitch;

    config_changed();
    disp_tof();
}

static void handle_save()
{
    save_request(true);
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

static void handle_nfc()
{
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)

    bool ret = pn532_config_sam();
    printf("Sam: %d\n", ret);

    uint8_t buf[32];

    int len = sizeof(buf);
    ret = pn532_poll_mifare(buf, &len);
    printf("Mifare: %d -", len);

    if (ret) {
        for (int i = 0; i < len; i++) {
            printf(" %02x", buf[i]);
        }
    }
    printf("\n");

    printf("Felica: ");
    if (pn532_poll_felica(buf, buf + 8, buf + 16, false)) {
        for (int i = 0; i < 18; i++) {
            printf(" %02x%s", buf[i], (i % 8 == 7) ? "," : "");
        }
    }
    printf("\n");
}

void handle_whoami()
{
    const char *msg[] = {"\nThis is Command Line port.\n",
                         "\nThis is Slider port.\n",
                         "\nThis is AIME port.\n"};
    for (int i = 0; i < 3; i++) {
        tud_cdc_n_write(i, msg[i], strlen(msg[i]));
        tud_cdc_n_write_flush(i);
    }
}

void commands_init()
{
    cli_register("display", handle_display, "Display all config.");
    cli_register("level", handle_level, "Set LED brightness level.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("tof", handle_tof, "Set ToF config.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
    cli_register("nfc", handle_nfc, "NFC debug.");
    cli_register("whoami", handle_whoami, "Tell each port.");
}
