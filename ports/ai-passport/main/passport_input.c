#include "passport_input.h"
#include <stddef.h>
#include <string.h>

int passport_input_event(PassportInput *state,unsigned slot,bool down,uint64_t now){
    if(!state || slot>=3) return PASSPORT_NO_ACTION;
    PassportButton *key=&state->buttons[slot];
    if(down){
        if(!key->down){key->down=true;key->held=false;key->started=now;}
        return PASSPORT_NO_ACTION;
    }
    if(!key->down) return PASSPORT_NO_ACTION;
    key->down=false;
    if(key->held) return PASSPORT_NO_ACTION;
    unsigned threshold=slot==1?PASSPORT_MIDDLE_HOLD_MS:PASSPORT_HOLD_MS;
    return (int)slot+(now-key->started>=threshold ? 3 : 0);
}
int passport_input_poll(PassportInput *state,uint64_t now){
    if(!state) return PASSPORT_NO_ACTION;
    for(unsigned slot=0;slot<3;slot++){
        PassportButton *key=&state->buttons[slot];
        unsigned threshold=slot==1?PASSPORT_MIDDLE_HOLD_MS:PASSPORT_HOLD_MS;
        if(key->down && !key->held && now-key->started>=threshold){
            key->held=true;return (int)slot+3;
        }
    }
    return PASSPORT_NO_ACTION;
}

int passport_landscape_slot(unsigned position,unsigned rotation){
    if(position>=3 || (rotation!=0 && rotation!=2))return PASSPORT_NO_ACTION;
    return (int)(rotation==2?2-position:position);
}
static void sync_rotation(PassportOrientedInput *state,unsigned rotation){
    if(state->rotation==rotation)return;
    // Old presses cannot acquire a new meaning halfway through a hold.
    memset(&state->logical,0,sizeof(state->logical));
    state->suppressed_mask|=state->down_mask;
    state->rotation=(uint8_t)rotation;
}
int passport_oriented_event(PassportOrientedInput *state,unsigned position,bool down,
                            unsigned rotation,uint64_t now){
    int slot=passport_landscape_slot(position,rotation);
    if(!state || slot<0)return PASSPORT_NO_ACTION;
    sync_rotation(state,rotation);
    uint8_t bit=(uint8_t)(1u<<position);
    if(down){
        if(state->down_mask&bit)return PASSPORT_NO_ACTION;
        state->down_mask|=bit;
    }else{
        if(!(state->down_mask&bit))return PASSPORT_NO_ACTION;
        state->down_mask&=(uint8_t)~bit;
        if(state->suppressed_mask&bit){
            state->suppressed_mask&=(uint8_t)~bit;
            return PASSPORT_NO_ACTION;
        }
    }
    return passport_input_event(&state->logical,(unsigned)slot,down,now);
}
int passport_oriented_poll(PassportOrientedInput *state,unsigned rotation,uint64_t now){
    if(!state || (rotation!=0 && rotation!=2))return PASSPORT_NO_ACTION;
    sync_rotation(state,rotation);
    return passport_input_poll(&state->logical,now);
}
