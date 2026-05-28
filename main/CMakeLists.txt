idf_component_register(
    SRCS         "main.c" "buttons.c" "menu.c" "icons.c"
    INCLUDE_DIRS "."
    REQUIRES     font8x8 sh1106 driver
)
