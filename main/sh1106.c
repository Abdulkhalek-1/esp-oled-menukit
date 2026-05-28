#include "sh1106.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Wiring — ESP32 DevKitC, HSPI bus.
#define PIN_SCK   15
#define PIN_MOSI  2
#define PIN_CS    18
#define PIN_DC    5
#define PIN_RES   4

#define SH1106_PAGES         (SH1106_HEIGHT / 8)   // 8 pages of 8 rows each.
#define SH1106_FB_SIZE       (SH1106_WIDTH * SH1106_PAGES)
#define SH1106_COL_OFFSET    2  // SH1106 has 132 columns; visible area starts at 2.

static uint8_t framebuffer[SH1106_FB_SIZE];
static spi_device_handle_t dev;

void sh1106_init(void)
{
    // 1. Configure RES and DC as outputs.
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_RES) | (1ULL << PIN_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    // 2. Pulse RES low → high to reset the controller.
    gpio_set_level(PIN_RES, 0);
    esp_rom_delay_us(10);              // datasheet: >=3us
    gpio_set_level(PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));     // let the chip come up

    // 3. Init the SPI bus (HSPI = SPI2_HOST).
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,             // OLED is write-only
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SH1106_WIDTH,  // one page at a time
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    // 4. Add the OLED as a device on that bus.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,  // 8 MHz — conservative
        .mode = 0,                          // CPOL=0, CPHA=0
        .spics_io_num = PIN_CS,             // SPI driver toggles CS
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &dev);
}

void sh1106_clear(void) { memset(framebuffer, 0, sizeof(framebuffer)); }
void sh1106_set_pixel(int x, int y, bool on) { (void)x; (void)y; (void)on; }
void sh1106_flush(void) {}
