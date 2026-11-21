include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/shared_sources.cmake")
travel_shared_sources(TRAVEL_VIEWPORT_SOURCES viewport)
idf_component_register(
    SRCS "lcd_init.c" "native_text.c" "native_icon.c" ${TRAVEL_VIEWPORT_SOURCES}
    INCLUDE_DIRS ".."
    REQUIRES GVIEW SPI
    PRIV_REQUIRES esp_timer
)

if(GVIEW_UI_AUDIT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE GVIEW_UI_AUDIT)
endif()
