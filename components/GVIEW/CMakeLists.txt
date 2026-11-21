include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/shared_sources.cmake")
travel_shared_sources(TRAVEL_RENDER_SOURCES renderer)
idf_component_register(SRCS ${TRAVEL_RENDER_SOURCES} INCLUDE_DIRS "..")
