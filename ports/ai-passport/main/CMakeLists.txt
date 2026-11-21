set(TRAVEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
include("${TRAVEL_ROOT}/cmake/shared_sources.cmake")
travel_shared_sources(TRAVEL_SOURCES app network)
idf_component_register(SRCS ${TRAVEL_SOURCES} "passport_main.c" "passport_input.c" "passport_controls.c" "display.c" "native_text.c" "overlay.c" "backlight.c" "audio.c" "mp3_player.cpp"
 INCLUDE_DIRS "../include" "${TRAVEL_ROOT}/main" "${TRAVEL_ROOT}/components"
 REQUIRES bsp GVIEW micro_mp3 spi_flash esp_wifi esp_http_client esp_netif esp_event nvs_flash json mbedtls esp_timer esp_hw_support esp_system esp_driver_gpio esp_driver_i2c esp_driver_usb_serial_jtag esp_partition)
target_compile_options(${COMPONENT_LIB} PRIVATE -funsigned-char -Wno-pointer-sign -Wno-incompatible-pointer-types -Wno-format -Wno-unused-variable -Wno-unused-function -Wno-int-conversion -Wno-missing-braces -Wno-unused-but-set-variable -Wno-implicit-fallthrough -Wno-misleading-indentation)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
file(GLOB AUDIO_INPUTS CONFIGURE_DEPENDS "${TRAVEL_ROOT}/main/assets/track_*.mp3")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/sounddata.bin"
 COMMAND ${PYTHON} "${CMAKE_CURRENT_LIST_DIR}/../tools/pack_sound.py" "${CMAKE_BINARY_DIR}/sounddata.bin"
 DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../tools/pack_sound.py" ${AUDIO_INPUTS} VERBATIM)
add_custom_target(passport_sound DEPENDS "${CMAKE_BINARY_DIR}/sounddata.bin")
add_dependencies(${COMPONENT_LIB} passport_sound)
esptool_py_flash_to_partition(flash "sounddata" "${CMAKE_BINARY_DIR}/sounddata.bin")
