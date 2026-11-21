#ifndef WIFI_PAGE_LAYOUT_H
#define WIFI_PAGE_LAYOUT_H

// Each target supplies geometry; network state, navigation and input stay shared.
#include "../pageDraw.h"
#include "wifi_layout_profile.h"

#define KEYBOARD_LEFT WIFI_CONTENT_LEFT
#define KEYBOARD_WIDTH WIFI_CONTENT_WIDTH
#define KEYBOARD_TOP 112
#define KEYBOARD_ROW_HEIGHT 22
#define KEYBOARD_KEY_HEIGHT 20
#define KEYBOARD_TABS_TOP 88
#define KEYBOARD_ACTIONS_TOP 200
#define KEYBOARD_ACTION_HEIGHT 20
#define KEYBOARD_TAB_PITCH (KEYBOARD_WIDTH / 4)
#define KEYBOARD_ACTION_PITCH (KEYBOARD_ACTION_WIDTH / 3)

#if WIFI_CONTENT_LEFT < WS_UI_LEFT || WIFI_CONTENT_LEFT + WIFI_CONTENT_WIDTH > WS_UI_RIGHT
#error "Wi-Fi content must stay inside the target viewport"
#endif

#endif
