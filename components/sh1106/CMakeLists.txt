idf_component_register(
    SRCS          "src/sh1106.c"
    INCLUDE_DIRS  "include"
    PRIV_REQUIRES driver esp_rom freertos font8x8
)
