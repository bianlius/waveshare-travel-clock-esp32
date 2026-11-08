#ifndef WIFI_KEYBOARD_H
#define WIFI_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_PASSWORD_MAX 63
#define WIFI_KEYBOARD_PAGES 4
#define WIFI_KEYBOARD_ACTIONS 3

typedef struct {
    uint8_t page;
    uint8_t focus;
} WifiKeyboard;

typedef enum {
    WIFI_KEY_CHARACTER,
    WIFI_KEY_DELETE,
    WIFI_KEY_SPACE,
    WIFI_KEY_CONNECT,
    WIFI_KEY_INVALID
} WifiKeyboardAction;

const char *wifiKeyboardCharacters(unsigned page);
size_t wifiKeyboardCharacterCount(unsigned page);
void wifiKeyboardMove(WifiKeyboard *keyboard, int direction);
void wifiKeyboardSwitch(WifiKeyboard *keyboard, int direction);
WifiKeyboardAction wifiKeyboardAction(const WifiKeyboard *keyboard);
bool wifiKeyboardCanActivate(const WifiKeyboard *keyboard, size_t passwordLength, bool canConnect);
char wifiKeyboardCharacter(const WifiKeyboard *keyboard);
bool wifiPasswordAppend(char *password, size_t capacity, char character);
bool wifiPasswordDelete(char *password);

#endif
