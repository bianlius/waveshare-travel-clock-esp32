idf_component_register(
    SRCS "lcd_init.c" "native_text.c" "native_icon.c" "viewport.c"
    INCLUDE_DIRS ".."
    REQUIRES GVIEW SPI
    PRIV_REQUIRES esp_timer
)

if(GVIEW_UI_AUDIT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE GVIEW_UI_AUDIT)
endif()
