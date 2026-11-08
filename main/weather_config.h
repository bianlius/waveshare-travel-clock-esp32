#ifndef GVIEW_WEATHER_CONFIG_H
#define GVIEW_WEATHER_CONFIG_H

#include <stddef.h>

/*
 * The repository never stores a real Seniverse API key.
 * Copy weather_config.local.example.h to weather_config.local.h and set it
 * locally when weather data is required.
 */
#define GVIEW_SENIVERSE_API_KEY NULL

#if defined(__has_include)
#if __has_include("weather_config.local.h")
#include "weather_config.local.h"
#endif
#endif

#endif
