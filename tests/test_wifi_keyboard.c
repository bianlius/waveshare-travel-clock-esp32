#include "../main/PAGE/INPUTTXT/wifi_keyboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    bool supported[128]={0};
    supported[' ']=true;
    const unsigned expected[]={26,26,10,32};
    for(unsigned page=0;page<WIFI_KEYBOARD_PAGES;++page){
        const char *characters=wifiKeyboardCharacters(page);
        assert(strlen(characters)==expected[page]);
        for(unsigned i=0;characters[i];++i){
            unsigned c=(unsigned char)characters[i];
            assert(c>='!' && c<='~' && !supported[c]);
            supported[c]=true;
        }
        WifiKeyboard keyboard={page,0};
        wifiKeyboardMove(&keyboard,-1);
        assert(wifiKeyboardAction(&keyboard)==WIFI_KEY_CONNECT);
        wifiKeyboardMove(&keyboard,1);
        assert(wifiKeyboardAction(&keyboard)==WIFI_KEY_CHARACTER);
        assert(wifiKeyboardCharacter(&keyboard)==characters[0]);
        keyboard.focus=expected[page]-1;
        wifiKeyboardMove(&keyboard,1);
        assert(wifiKeyboardAction(&keyboard)==WIFI_KEY_DELETE);
        wifiKeyboardMove(&keyboard,1);
        assert(wifiKeyboardAction(&keyboard)==WIFI_KEY_SPACE);
        wifiKeyboardMove(&keyboard,1);
        assert(wifiKeyboardAction(&keyboard)==WIFI_KEY_CONNECT);
        wifiKeyboardMove(&keyboard,1);
        assert(keyboard.focus==0);
    }
    for(unsigned c=' ';c<='~';++c) assert(supported[c]);
    WifiKeyboard keyboard={0,20};
    wifiKeyboardSwitch(&keyboard,-1);
    assert(keyboard.page==3 && keyboard.focus==0);
    wifiKeyboardSwitch(&keyboard,1);
    assert(keyboard.page==0 && keyboard.focus==0);

    keyboard=(WifiKeyboard){0,0};
    assert(wifiKeyboardCanActivate(&keyboard,0,true));
    assert(wifiKeyboardCanActivate(&keyboard,62,true));
    assert(!wifiKeyboardCanActivate(&keyboard,63,true));
    keyboard.focus=26;
    assert(!wifiKeyboardCanActivate(&keyboard,0,true));
    assert(wifiKeyboardCanActivate(&keyboard,1,true));
    keyboard.focus=27;
    assert(wifiKeyboardCanActivate(&keyboard,62,true));
    assert(!wifiKeyboardCanActivate(&keyboard,63,true));
    keyboard.focus=28;
    assert(!wifiKeyboardCanActivate(&keyboard,8,false));
    assert(wifiKeyboardCanActivate(&keyboard,0,true));
    assert(wifiKeyboardCanActivate(&keyboard,63,true));
    keyboard.focus=29;
    assert(!wifiKeyboardCanActivate(&keyboard,0,true));

    struct { char password[64]; unsigned guard; } buffer={{0},0x12345678};
    assert(!wifiPasswordDelete(buffer.password));
    for(unsigned i=0;i<WIFI_PASSWORD_MAX;++i)
        assert(wifiPasswordAppend(buffer.password,sizeof(buffer.password),'a'));
    assert(strlen(buffer.password)==63 && buffer.guard==0x12345678);
    assert(!wifiPasswordAppend(buffer.password,sizeof(buffer.password),'b'));
    assert(strlen(buffer.password)==63 && buffer.guard==0x12345678);
    assert(wifiPasswordDelete(buffer.password));
    assert(wifiPasswordAppend(buffer.password,sizeof(buffer.password),' '));
    assert(buffer.password[62]==' ' && buffer.password[63]==0);
    while(wifiPasswordDelete(buffer.password)) {}
    const char *symbols=wifiKeyboardCharacters(3);
    for(unsigned i=0;symbols[i];++i)
        assert(wifiPasswordAppend(buffer.password,sizeof(buffer.password),symbols[i]));
    assert(strcmp(buffer.password,symbols)==0);
    assert(!wifiPasswordAppend(buffer.password,sizeof(buffer.password),'\n'));
    char small[3]={0};
    assert(wifiPasswordAppend(small,sizeof(small),'a'));
    assert(wifiPasswordAppend(small,sizeof(small),'b'));
    assert(!wifiPasswordAppend(small,sizeof(small),'c'));
    assert(strcmp(small,"ab")==0);
    puts("Wi-Fi keyboard coverage, navigation and password bounds passed");
}
