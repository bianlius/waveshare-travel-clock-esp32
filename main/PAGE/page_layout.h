#ifndef GVIEW_PAGE_LAYOUT_H
#define GVIEW_PAGE_LAYOUT_H

// Only coordinates vary by target; page state and actions remain shared.
#include "page_layout_profile.h"
#define PAGE_CONTENT_RIGHT (PAGE_CONTENT_LEFT + PAGE_CONTENT_WIDTH)
#define PAGE_CALENDAR_COLUMN_WIDTH (PAGE_CONTENT_WIDTH / 7)
#define PAGE_CITY_TEXT_WIDTH (PAGE_CITY_COLUMN_WIDTH - 10)
#define PAGE_WEATHER_CARD_WIDTH ((WS_UI_WIDTH - 12 - 8*(PAGE_WEATHER_COLUMNS-1)) / PAGE_WEATHER_COLUMNS)
#define PAGE_WEATHER_CARD_PITCH (PAGE_WEATHER_CARD_WIDTH + 8)

#if PAGE_WEATHER_FIRST_DAY < 0 || PAGE_WEATHER_FIRST_DAY + PAGE_WEATHER_COLUMNS > 3
#error "Weather cards must use the three available forecast days"
#endif

#if PAGE_CONTENT_LEFT < WS_UI_LEFT || PAGE_CONTENT_RIGHT > WS_UI_RIGHT
#error "Page content must stay inside its target viewport"
#endif

#endif
