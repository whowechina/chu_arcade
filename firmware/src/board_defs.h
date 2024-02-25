/*
 * Chu Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_CHU_ARCADE

#define I2C_PORT i2c1
#define I2C_SDA 26
#define I2C_SCL 27
#define I2C_FREQ 1333*1000

#define TOF_MUX_LIST { 3, 4, 5, 2, 1, 0, 11, 12, 13, 10, 9, 8}

#define RGB_PIN 28
#define RGB_ORDER GRB // or RGB

#define SLIDER_TX 0
#define SLIDER_RX 1
#define SLIDER_UART uart0

#define NKRO_KEYMAP "1aqz2swx3dec4frv5gtb6hyn7jum8ki90olp,."

#define BUTTON_DEF { 3, 4, 5}
#else

#endif
