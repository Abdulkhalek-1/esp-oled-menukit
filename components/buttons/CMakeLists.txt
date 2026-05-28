idf_component_register(
    SRCS          "src/buttons.c"
    INCLUDE_DIRS  "include"
    REQUIRES      freertos
    PRIV_REQUIRES driver esp_common
)
