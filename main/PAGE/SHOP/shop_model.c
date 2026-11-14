#include "shop_model.h"
#include <stddef.h>
void shopBuildPriceOrder(const uint16_t *costs, uint8_t count, uint8_t *order){
    if(!costs || !order) return;
    for(uint8_t i=0;i<count;++i){
        uint8_t position=i;
        while(position && costs[order[position-1]]>costs[i]){
            order[position]=order[position-1];
            --position;
        }
        order[position]=i;
    }
}
ShopActionResult shopApply(uint64_t *owned, uint8_t *equipped, uint8_t item, uint16_t cost, uint32_t *balance){
    if(!owned || !balance || item >= 64) return SHOP_INVALID;
    uint64_t bit = UINT64_C(1) << item;
    if(!(*owned & bit)){
        if(*balance < cost) return SHOP_NOT_ENOUGH;
        *balance -= cost;
        *owned |= bit;
        return SHOP_UNLOCKED;
    }
    if(!equipped || *equipped == item) return SHOP_ALREADY_OWNED;
    *equipped = item;
    return SHOP_EQUIPPED;
}

uint8_t shopRemoveRetiredMonster(uint64_t *owned,uint8_t *equipped){
    if(!owned || !equipped) return 0;
    const uint64_t retired=UINT64_C(1)<<SHOP_RETIRED_BRELOOM;
    const uint8_t changed=(*owned & retired)!=0 || *equipped==SHOP_RETIRED_BRELOOM;
    *owned &= ~retired;
    if(*equipped==SHOP_RETIRED_BRELOOM) *equipped=0;
    return changed;
}
