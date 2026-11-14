#include "wifi_ssid.h"

#include <stdint.h>
#include <string.h>

#include "GVIEW/utf8_gb2312_switch.h"

static bool validUtf8(const unsigned char *text, size_t length){
    for(size_t i=0;i<length;){
        unsigned char first=text[i];
        if(first<0x80){ ++i; continue; }
        size_t width=first>=0xC2 && first<=0xDF ? 2 :
                     first>=0xE0 && first<=0xEF ? 3 :
                     first>=0xF0 && first<=0xF4 ? 4 : 0;
        if(!width || i+width>length) return false;
        for(size_t j=1;j<width;++j)
            if((text[i+j]&0xC0)!=0x80) return false;
        if(width==3 && ((first==0xE0 && text[i+1]<0xA0) ||
                        (first==0xED && text[i+1]>=0xA0))) return false;
        if(width==4 && ((first==0xF0 && text[i+1]<0x90) ||
                        (first==0xF4 && text[i+1]>=0x90))) return false;
        i+=width;
    }
    return true;
}

static bool validLegacyGb(const unsigned char *text, size_t length){
    bool hasWide=false;
    for(size_t i=0;i<length;){
        if(text[i]<0x80){ ++i; continue; }
        if(i+1>=length || text[i]<0x81 || text[i]>0xFE ||
           text[i+1]<0x40 || text[i+1]>0xFE || text[i+1]==0x7F)
            return false;
        hasWide=true;
        i+=2;
    }
    return hasWide;
}

bool wifiSsidUpgradeLegacy(char ssid[33]){
    size_t length=strnlen(ssid,33);
    if(length==0 || length==33 || validUtf8((const unsigned char *)ssid,length) ||
       !validLegacyGb((const unsigned char *)ssid,length)) return false;

    // The old firmware saved screen-encoded bytes. Convert only when the
    // mapping round-trips and the original 32-byte Wi-Fi limit is respected.
    uint8_t utf8[97]={0};
    size_t converted=gb2312_to_utf8((const uint8_t *)ssid,length,utf8,sizeof(utf8));
    if(converted==0 || converted>32 || !validUtf8(utf8,converted)) return false;
    uint8_t roundTrip[65]={0};
    size_t restored=utf8_to_gb2312(utf8,converted,roundTrip,sizeof(roundTrip));
    if(restored!=length || memcmp(roundTrip,ssid,length)!=0) return false;
    memcpy(ssid,utf8,converted);
    ssid[converted]=0;
    return true;
}

void wifiSsidToDisplay(const char ssid[33], char *display, size_t capacity){
    if(!capacity) return;
    display[0]=0;
    size_t length=strnlen(ssid,33);
    if(length==33) return;
    if(validUtf8((const unsigned char *)ssid,length)){
        // A 32-byte UTF-8 SSID produces at most 32 GB2312 bytes.
        if(capacity<33) return;
        size_t written=utf8_to_gb2312((const uint8_t *)ssid,length,
                                      (uint8_t *)display,capacity);
        display[written]=0;
    }else if(validLegacyGb((const unsigned char *)ssid,length) && length<capacity){
        // Keep older records legible even when they cannot be migrated.
        memcpy(display,ssid,length+1);
    }
}
