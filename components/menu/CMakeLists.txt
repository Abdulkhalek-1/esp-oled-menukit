idf_component_register(
    SRCS          "src/menu.c"
    INCLUDE_DIRS  "include"
    REQUIRES      buttons freertos
    PRIV_REQUIRES sh1106 font8x8
)
