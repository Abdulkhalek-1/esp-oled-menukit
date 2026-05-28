idf_component_register(
    SRCS         "sh1106.c" "main.c" "buttons.c" "menu.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8 driver
)
