#include "st7735.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include <string.h>

#define PIN_CS   33
#define PIN_RST  25
#define PIN_DC   26
#define PIN_MOSI 27
#define PIN_SCK  14

#define LCD_WIDTH  130
#define LCD_HEIGHT 160


static spi_device_handle_t spi;

static const uint8_t font[96][5] = {
    // Space (ASCII 32)
    [0] = { 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' '
    // Symbols
    [10] = { 0x00, 0x36, 0x36, 0x00, 0x00 }, // ':' (ASCII 58 - 32 = index 26)
    [14] = { 0x10, 0x28, 0x7C, 0x28, 0x10 }, // '%' (ASCII 37 - 32 = index 5)
    [14] = { 0x00, 0x00, 0x60, 0x60, 0x00 }, // '.' (ASCII 46 - 32 = index 14)

    // Numbers
    [16] = { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // '0'
    [17] = { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // '1'
    [18] = { 0x42, 0x61, 0x51, 0x49, 0x46 }, // '2'
    [19] = { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // '3'
    [20] = { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // '4'
    [21] = { 0x27, 0x45, 0x45, 0x45, 0x39 }, // '5'
    [22] = { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // '6'
    [23] = { 0x01, 0x71, 0x09, 0x05, 0x03 }, // '7'
    [24] = { 0x36, 0x49, 0x49, 0x49, 0x36 }, // '8'
    [25] = { 0x06, 0x49, 0x49, 0x29, 0x1E }, // '9'

    // Uppercase letters (ASCII - 32)
    [33] = { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 'A'
    [37] = { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 'E'
    [38] = { 0x7F, 0x09, 0x09, 0x09, 0x01 }, // 'F'
    [44] = { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 'L'
    [45] = { 0x7F, 0x04, 0x08, 0x04, 0x7F }, // 'M'
    [46] = { 0x7F, 0x02, 0x04, 0x08, 0x7F }, // Correct 'N'
    [47] = { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 'O'
    [50] = { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 'R'
    [51] = { 0x46, 0x49, 0x49, 0x49, 0x31 }, // 'S'
    [57] = { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 'Y'

    // Lowercase letters
    [65] = { 0x20, 0x54, 0x54, 0x54, 0x78 }, // 'a'
    [69] = { 0x38, 0x54, 0x54, 0x54, 0x18 }, // 'e'
    [76] = { 0x00, 0x41, 0x7F, 0x40, 0x00 }, // 'l'
    [89] = { 0x0C, 0x50, 0x50, 0x50, 0x3C }, // 'y'
};


static void lcd_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
    spi_device_transmit(spi, &t);
}

static void lcd_data(const uint8_t *data, int len) {
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {.length = len * 8, .tx_buffer = data};
    spi_device_transmit(spi, &t);
}

void st7735_init() {
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(HSPI_HOST, &devcfg, &spi);

    // Reset sequence
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // ST7735S init sequence (Red Tab)
    lcd_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150)); // Software reset
    lcd_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(150)); // Sleep out

    lcd_cmd(0x3A); // Color mode
    uint8_t data = 0x05; // 16-bit
    lcd_data(&data, 1);

    lcd_cmd(0x36); // MADCTL
    data = 0xC0;   // Row/col exchange
    lcd_data(&data, 1);

    lcd_cmd(0x29); // Display ON
    vTaskDelay(pdMS_TO_TICKS(10));
}


void st7735_fill_screen(uint16_t color) {
    lcd_cmd(0x2A); // col addr
    uint8_t data_col[] = {0, 0, 0, LCD_WIDTH - 1};
    lcd_data(data_col, 4);

    lcd_cmd(0x2B); // row addr
    uint8_t data_row[] = {0, 0, (LCD_HEIGHT - 1) >> 8, (LCD_HEIGHT - 1) & 0xFF};
    lcd_data(data_row, 4);

    lcd_cmd(0x2C); // RAM write
    uint8_t pixel[2] = {color >> 8, color & 0xFF};
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        lcd_data(pixel, 2);
    }
}


void st7735_draw_pixel(int x, int y, uint16_t color) {
    uint8_t col[] = {0x00, x, 0x00, x};
    uint8_t row[] = {0x00, y, 0x00, y};
    uint8_t pixel[] = {color >> 8, color & 0xFF};

    lcd_cmd(0x2A);
    lcd_data(col, 4);
    lcd_cmd(0x2B);
    lcd_data(row, 4);
    lcd_cmd(0x2C);
    lcd_data(pixel, 2);
}

void st7735_draw_text(int x, int y, const char *text, uint16_t color, uint16_t bg, int scale) {
    for (int i = 0; text[i]; i++) {
        char c = text[i];
        if (c < 32 || c > 127) c = '?';
        for (int col = 0; col < 5; col++) {
            uint8_t line = font[c - 32][col];
            for (int row = 0; row < 8; row++) {
                uint16_t px = (line >> row) & 0x01 ? color : bg;
                for (int dx = 0; dx < scale; dx++) {
                    for (int dy = 0; dy < scale; dy++) {
                        int draw_x = x + (i * 6 + col) * scale + dx;
                        int draw_y = y + row * scale + dy;
                        if (draw_x < LCD_WIDTH && draw_y < LCD_HEIGHT) {
                            st7735_draw_pixel(draw_x, draw_y, px);
                        }
                    }
                }
            }
        }
    }
}

void st7735_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    int x_end = x + w;
    int y_end = y + h;

    if (x_end > LCD_WIDTH) x_end = LCD_WIDTH;
    if (y_end > LCD_HEIGHT) y_end = LCD_HEIGHT;

    for (int i = y; i < y_end; i++) {
        for (int j = x; j < x_end; j++) {
            st7735_draw_pixel(j, i, color);
        }
    }
}
