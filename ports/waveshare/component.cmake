get_filename_component(TRAVEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(WAVESHARE_PORT "${CMAKE_CURRENT_LIST_DIR}")
include("${TRAVEL_ROOT}/cmake/shared_sources.cmake")
travel_shared_sources(TRAVEL_SHARED_SOURCES app network)

idf_component_register(SRCS ${TRAVEL_SHARED_SOURCES}
    "${WAVESHARE_PORT}/main.c" "${WAVESHARE_PORT}/backlight.c"
    "${WAVESHARE_PORT}/adc.c" "${WAVESHARE_PORT}/imu_rotation.c"
    "${WAVESHARE_PORT}/sound.c" "${WAVESHARE_PORT}/waveshare_audio.c"
    "${WAVESHARE_PORT}/mp3_player.c"
    INCLUDE_DIRS "${TRAVEL_ROOT}/main" "${WAVESHARE_PORT}/include")

if(GVIEW_UI_AUDIT)
    target_sources(${COMPONENT_LIB} PRIVATE "${TRAVEL_ROOT}/tools/ui_audit.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE GVIEW_UI_AUDIT)
endif()
