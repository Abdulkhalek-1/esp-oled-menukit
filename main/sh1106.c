#include "sh1106.h"

#include "esp_rom_sys.h"
#include "font8x8.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Wiring — ESP32 DevKitC, HSPI bus.
#define PIN_SCK 15
#define PIN_MOSI 2
#define PIN_CS 5
#define PIN_DC 18
#define PIN_RES 4

#define SH1106_PAGES (SH1106_HEIGHT / 8) // 8 pages of 8 rows each.
#define SH1106_FB_SIZE (SH1106_WIDTH * SH1106_PAGES)
#define SH1106_COL_OFFSET 2 // SH1106 has 132 columns; visible area starts at 2.

static uint8_t             framebuffer[SH1106_FB_SIZE];
static spi_device_handle_t dev;

static void                sh1106_cmd(uint8_t c)
{
    gpio_set_level(PIN_DC, 0); // 0 = command
    spi_transaction_t t = {
        .length    = 8, // bits, not bytes
        .tx_buffer = &c,
    };
    spi_device_polling_transmit(dev, &t);
}

static void sh1106_data(const uint8_t *buf, size_t len)
{
    gpio_set_level(PIN_DC, 1); // 1 = data
    spi_transaction_t t = {
        .length    = len * 8, // bits
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(dev, &t);
}

esp_err_t sh1106_init(void)
{
    // 1. Configure RES and DC as outputs.
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_RES) | (1ULL << PIN_DC),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    // 2. Pulse RES low → high to reset the controller.
    gpio_set_level(PIN_RES, 0);
    esp_rom_delay_us(10); // datasheet: >=3us
    gpio_set_level(PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // let the chip come up

    // 3. Init the SPI bus (HSPI = SPI2_HOST).
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = -1, // OLED is write-only
        .sclk_io_num     = PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = SH1106_WIDTH, // one page at a time
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    // 4. Add the OLED as a device on that bus.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000, // 8 MHz — conservative
        .mode           = 0,               // CPOL=0, CPHA=0
        .spics_io_num   = PIN_CS,          // SPI driver toggles CS
        .queue_size     = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &devcfg, &dev);
    if (err != ESP_OK) return err;

    // 5. SH1106 power-on init sequence.
    sh1106_cmd(0xAE); // display off
    sh1106_cmd(0xD5);
    sh1106_cmd(0x80); // clock divide ratio / oscillator freq
    sh1106_cmd(0xA8);
    sh1106_cmd(0x3F); // multiplex ratio = 64
    sh1106_cmd(0xD3);
    sh1106_cmd(0x00); // display offset = 0
    sh1106_cmd(0x40); // display start line = 0
    sh1106_cmd(0xAD);
    sh1106_cmd(0x8B); // DC-DC charge pump ON (SH1106-specific; do NOT use SSD1306's 0x8D 0x14)
    sh1106_cmd(0xA1); // segment remap: column 127 -> SEG0
    sh1106_cmd(0xC8); // COM scan direction: remapped (top-to-bottom flipped)
    sh1106_cmd(0xDA);
    sh1106_cmd(0x12); // COM pins hardware config
    sh1106_cmd(0x81);
    sh1106_cmd(0x80); // contrast = 128/255
    sh1106_cmd(0xD9);
    sh1106_cmd(0x22); // pre-charge period
    sh1106_cmd(0xDB);
    sh1106_cmd(0x35); // VCOMH deselect level
    sh1106_cmd(0xA4); // resume to RAM content (not all-on)
    sh1106_cmd(0xA6); // normal display (not inverted)
    sh1106_cmd(0xAF); // display on

    return ESP_OK;
}

void sh1106_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}
void sh1106_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SH1106_WIDTH) return;
    if (y < 0 || y >= SH1106_HEIGHT) return;

    // Page-major framebuffer: each byte holds 8 vertically-stacked pixels of one page.
    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * SH1106_WIDTH + x;

    if (on)
        framebuffer[idx] |= (1 << bit);
    else
        framebuffer[idx] &= ~(1 << bit);
}
void sh1106_flush(void)
{
    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        sh1106_cmd(0xB0 | page);                              // set page address (0xB0-0xB7)
        sh1106_cmd(0x00 | (SH1106_COL_OFFSET & 0x0F));        // lower column nibble  = 2
        sh1106_cmd(0x10 | ((SH1106_COL_OFFSET >> 4) & 0x0F)); // higher column nibble = 0
        sh1106_data(&framebuffer[page * SH1106_WIDTH], SH1106_WIDTH);
    }
}

static void draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font8x8[uc - 32];

    for (int col = 0; col < 8; col++) {
        uint8_t bits = glyph[col]; // one column of the glyph
        for (int row = 0; row < 8; row++) {
            if (bits & (1 << row)) { // LSB = top, so bit `row` = pixel row
                sh1106_set_pixel(x + col, y + row, true);
            }
        }
    }
}

void sh1106_draw_string(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x, y, *s++);
        x += 8;
    }
}
