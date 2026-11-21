#include "audio_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    for(unsigned rings=1;rings<=20;rings++){
        uint16_t count=rings+1;unsigned played=0;
        while(count){
            count=soundAlarmStarted(count);++played;
            count=soundAlarmFinished(count);
            assert(played<=rings);
        }
        assert(played==rings);
    }
    uint16_t count=101;
    for(unsigned i=0;i<1000;i++)count=soundAlarmFinished(soundAlarmStarted(count));
    assert(count==101);
    assert(soundAlarmStarted(0)==0 && soundAlarmFinished(0)==0);
    puts("PASS: exact finite ring count, unlimited ringing, and cancellation without underflow");
}
