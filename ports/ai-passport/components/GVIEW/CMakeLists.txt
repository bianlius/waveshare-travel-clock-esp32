set(TRAVEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
include("${TRAVEL_ROOT}/cmake/shared_sources.cmake")
travel_shared_sources(TRAVEL_RENDER_SOURCES renderer viewport)
idf_component_register(SRCS ${TRAVEL_RENDER_SOURCES}
 INCLUDE_DIRS "${TRAVEL_ROOT}/components" "${TRAVEL_ROOT}/components/GVIEW" "../../include")
target_compile_options(${COMPONENT_LIB} PRIVATE -funsigned-char -Wno-pointer-sign -Wno-incompatible-pointer-types -Wno-format -Wno-unused-variable -Wno-unused-function -Wno-int-conversion -Wno-missing-braces -Wno-unused-but-set-variable -Wno-implicit-fallthrough -Wno-misleading-indentation)
