#include "passport_input.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    for(unsigned slot=0;slot<3;slot++){
        const unsigned thresholds[]={650,1500,650};
        const unsigned threshold=thresholds[slot];
        PassportInput input={0};
        assert(passport_input_event(&input,slot,false,1)==-1);
        assert(passport_input_event(&input,slot,true,10)==-1);
        assert(passport_input_event(&input,slot,true,20)==-1);
        assert(passport_input_poll(&input,10+threshold-1)==-1);
        assert(passport_input_event(&input,slot,false,10+threshold-1)==(int)slot);
        assert(passport_input_event(&input,slot,false,10+threshold)==-1);
        assert(passport_input_event(&input,slot,true,3000)==-1);
        assert(passport_input_poll(&input,3000+threshold-1)==-1);
        assert(passport_input_poll(&input,3000+threshold)==(int)slot+3);
        assert(passport_input_poll(&input,5000+threshold)==-1);
        assert(passport_input_event(&input,slot,false,5000+threshold)==-1);
        assert(passport_input_event(&input,slot,true,8000)==-1);
        assert(passport_input_event(&input,slot,false,8000+threshold)==(int)slot+3);
        assert(passport_input_poll(&input,11000)==-1);
    }
    PassportInput input={0};
    assert(passport_input_event(&input,3,true,0)==-1);
    assert(passport_input_event(NULL,0,true,0)==-1);
    assert(passport_input_poll(NULL,0)==-1);
    for(unsigned rotation=0;rotation<=2;rotation+=2){
        for(unsigned position=0;position<3;++position){
            unsigned slot=rotation==2?2-position:position;
            unsigned threshold=slot==1?1500:650;
            PassportOrientedInput board={0};
            assert(passport_landscape_slot(position,rotation)==(int)slot);
            assert(passport_oriented_event(&board,position,true,rotation,10)==-1);
            assert(passport_oriented_event(&board,position,false,rotation,20)==(int)slot);
            assert(passport_oriented_event(&board,position,true,rotation,100)==-1);
            assert(passport_oriented_poll(&board,rotation,100+threshold-1)==-1);
            assert(passport_oriented_poll(&board,rotation,100+threshold)==(int)slot+3);
            assert(passport_oriented_event(&board,position,false,rotation,3000)==-1);
        }
    }
    for(unsigned position=0;position<3;++position){
        PassportOrientedInput board={0};
        assert(passport_oriented_event(&board,position,true,0,0)==-1);
        assert(passport_oriented_poll(&board,2,2000)==-1);
        assert(passport_oriented_event(&board,position,true,2,2001)==-1);
        assert(passport_oriented_event(&board,position,false,2,2002)==-1);
        assert(passport_oriented_poll(&board,2,4000)==-1);
        assert(passport_oriented_event(&board,position,true,2,5000)==-1);
        assert(passport_oriented_event(&board,position,false,2,5010)==(int)(2-position));
    }
    PassportOrientedInput board={0};
    assert(passport_oriented_event(&board,0,true,0,0)==-1);
    assert(passport_oriented_event(&board,2,true,0,1)==-1);
    assert(passport_oriented_event(&board,2,false,2,20)==-1);
    assert(passport_oriented_event(&board,0,false,2,21)==-1);
    assert(passport_oriented_poll(&board,2,3000)==-1);
    assert(passport_landscape_slot(3,0)==-1 && passport_landscape_slot(0,1)==-1);
    assert(passport_oriented_event(NULL,0,true,0,0)==-1);
    assert(passport_oriented_poll(NULL,0,0)==-1);
    puts("PASS: Waveshare 650 ms side/1500 ms middle holds, held polling, release suppression and invalid events");
    puts("PASS: physical landscape positions in both directions; held keys cancelled safely on rotation");
}
