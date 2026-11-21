#include "passport_audio.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    for(unsigned rings=1;rings<=20;rings++){
        uint16_t count=rings+1;unsigned played=0;
        while(count){
            count=passport_alarm_started(count);++played;
            count=passport_alarm_finished(count);
            assert(played<=rings);
        }
        assert(played==rings);
    }
    uint16_t count=101;
    for(unsigned i=0;i<1000;i++)count=passport_alarm_finished(passport_alarm_started(count));
    assert(count==101);
    assert(passport_alarm_started(0)==0 && passport_alarm_finished(0)==0);
    puts("PASS: exact finite ring count, unlimited ringing, and cancellation without underflow");
}
