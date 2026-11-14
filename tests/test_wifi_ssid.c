#include <assert.h>
#include <string.h>

#include "../main/wifi_ssid.h"

int main(void){
    char legacy[33]={ 'H','o','m','e','-',0xD6,0xD0,0xCE,0xC4,0 };
    const char utf8[]="Home-\xE4\xB8\xAD\xE6\x96\x87";
    char display[66];

    wifiSsidToDisplay(legacy,display,sizeof(display));
    assert(memcmp(display,"Home-\xD6\xD0\xCE\xC4",10)==0);
    assert(wifiSsidUpgradeLegacy(legacy));
    assert(strcmp(legacy,utf8)==0);
    assert(!wifiSsidUpgradeLegacy(legacy));
    wifiSsidToDisplay(legacy,display,sizeof(display));
    assert(memcmp(display,"Home-\xD6\xD0\xCE\xC4",10)==0);

    char ascii[33]="Guest_5G";
    assert(!wifiSsidUpgradeLegacy(ascii));
    wifiSsidToDisplay(ascii,display,sizeof(display));
    assert(strcmp(display,ascii)==0);

    char invalid[33]={ 'A',0xFF,0 };
    assert(!wifiSsidUpgradeLegacy(invalid));
    wifiSsidToDisplay(invalid,display,sizeof(display));
    assert(display[0]==0);

    // Sixteen GB2312 characters would expand beyond the 32-byte SSID limit.
    char tooLong[33];
    for(int i=0;i<16;++i){ tooLong[2*i]=(char)0xD6; tooLong[2*i+1]=(char)0xD0; }
    tooLong[32]=0;
    assert(!wifiSsidUpgradeLegacy(tooLong));
    wifiSsidToDisplay(tooLong,display,sizeof(display));
    assert(memcmp(display,tooLong,33)==0);
    return 0;
}
