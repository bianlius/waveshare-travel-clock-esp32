#ifndef WIFI_SSID_H
#define WIFI_SSID_H

#include <stdbool.h>
#include <stddef.h>

// Stored SSIDs are the original Wi-Fi bytes (at most 32 bytes plus NUL).
bool wifiSsidUpgradeLegacy(char ssid[33]);
void wifiSsidToDisplay(const char ssid[33], char *display, size_t capacity);

#endif
