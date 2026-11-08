#include "wifi_keyboard.h"
#include <string.h>

static const char *const characters[WIFI_KEYBOARD_PAGES] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "1234567890",
    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
};

const char *wifiKeyboardCharacters(unsigned page){
    return page < WIFI_KEYBOARD_PAGES ? characters[page] : "";
}

size_t wifiKeyboardCharacterCount(unsigned page){
    return strlen(wifiKeyboardCharacters(page));
}

void wifiKeyboardMove(WifiKeyboard *keyboard, int direction){
    unsigned count=wifiKeyboardCharacterCount(keyboard->page)+WIFI_KEYBOARD_ACTIONS;
    keyboard->focus=(keyboard->focus+count+(direction < 0 ? -1 : 1))%count;
}

void wifiKeyboardSwitch(WifiKeyboard *keyboard, int direction){
    keyboard->page=(keyboard->page+WIFI_KEYBOARD_PAGES+(direction < 0 ? -1 : 1))%WIFI_KEYBOARD_PAGES;
    keyboard->focus=0;
}

WifiKeyboardAction wifiKeyboardAction(const WifiKeyboard *keyboard){
    unsigned count=wifiKeyboardCharacterCount(keyboard->page);
    if(keyboard->focus < count) return WIFI_KEY_CHARACTER;
    unsigned action=keyboard->focus-count;
    return action < WIFI_KEYBOARD_ACTIONS ? (WifiKeyboardAction)(WIFI_KEY_DELETE+action) : WIFI_KEY_INVALID;
}

bool wifiKeyboardCanActivate(const WifiKeyboard *keyboard,size_t passwordLength,bool canConnect){
    if(keyboard->page>=WIFI_KEYBOARD_PAGES) return false;
    switch(wifiKeyboardAction(keyboard)){
        case WIFI_KEY_CHARACTER:
        case WIFI_KEY_SPACE: return passwordLength<WIFI_PASSWORD_MAX;
        case WIFI_KEY_DELETE: return passwordLength>0;
        case WIFI_KEY_CONNECT: return canConnect;
        default: return false;
    }
}

char wifiKeyboardCharacter(const WifiKeyboard *keyboard){
    return keyboard->focus < wifiKeyboardCharacterCount(keyboard->page) ?
        wifiKeyboardCharacters(keyboard->page)[keyboard->focus] : 0;
}

bool wifiPasswordAppend(char *password, size_t capacity, char character){
    size_t length=strlen(password);
    if(character < ' ' || character > '~' || length >= WIFI_PASSWORD_MAX || length+1 >= capacity)
        return false;
    password[length]=character;
    password[length+1]=0;
    return true;
}

bool wifiPasswordDelete(char *password){
    size_t length=strlen(password);
    if(!length) return false;
    password[length-1]=0;
    return true;
}
