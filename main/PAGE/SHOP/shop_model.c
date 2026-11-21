#include "shop_model.h"
#include <stddef.h>
static const uint64_t retiredMonsterMask=
    (UINT64_C(1)<<SHOP_RETIRED_ZIGZAGOON) | (UINT64_C(1)<<SHOP_RETIRED_BRELOOM) |
    (UINT64_C(1)<<SHOP_RETIRED_VOLBEAT) | (UINT64_C(1)<<SHOP_RETIRED_MEOWTH) |
    (UINT64_C(1)<<SHOP_RETIRED_LUGIA) | (UINT64_C(1)<<SHOP_RETIRED_HO_OH) |
    (UINT64_C(1)<<SHOP_RETIRED_PIDGEOT) | (UINT64_C(1)<<SHOP_RETIRED_REGICE) |
    (UINT64_C(1)<<SHOP_RETIRED_LUCARIO);

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
uint8_t shopIsRetiredMonster(uint8_t item){
    return item<64 && (retiredMonsterMask & (UINT64_C(1)<<item))!=0;
}

void shopBuildMonsterOrder(const uint16_t *costs,uint8_t count,uint8_t *order){
    if(!costs || !order)return;
    const uint8_t prefix=count<SHOP_FIRST_ADDED_FOLLOWER?count:SHOP_FIRST_ADDED_FOLLOWER;
    // Keep "none" and the nine original companions in their original order.
    for(uint8_t i=0;i<prefix;++i)order[i]=i;
    shopBuildPriceOrder(costs+prefix,count-prefix,order+prefix);
    for(uint8_t i=prefix;i<count;++i)order[i]+=prefix;
}

uint8_t shopHasItem(uint64_t owned,uint64_t extra,uint8_t item){
    return item<SHOP_MONSTER_CAPACITY &&
           ((item<64 ? owned : extra) & (UINT64_C(1)<<(item%64)))!=0;
}

ShopActionResult shopApplyExtended(uint64_t *owned,uint64_t *extra,uint8_t *equipped,
                                   uint8_t item,uint16_t cost,uint32_t *balance){
    if(!owned || !balance || item>=SHOP_MONSTER_CAPACITY || (item>=64 && !extra))
        return SHOP_INVALID;
    uint64_t *word=item<64 ? owned : extra;
    const uint64_t bit=UINT64_C(1)<<(item%64);
    if(!(*word & bit)){
        if(*balance < cost) return SHOP_NOT_ENOUGH;
        *balance -= cost;
        *word |= bit;
        return SHOP_UNLOCKED;
    }
    if(!equipped || *equipped == item) return SHOP_ALREADY_OWNED;
    *equipped = item;
    return SHOP_EQUIPPED;
}

ShopActionResult shopApply(uint64_t *owned,uint8_t *equipped,uint8_t item,uint16_t cost,uint32_t *balance){
    return shopApplyExtended(owned,NULL,equipped,item,cost,balance);
}

uint8_t shopRemoveRetiredMonster(uint64_t *owned,uint8_t *equipped){
    if(!owned || !equipped) return 0;
    const uint8_t changed=(*owned & retiredMonsterMask)!=0 || shopIsRetiredMonster(*equipped);
    *owned &= ~retiredMonsterMask;
    if(shopIsRetiredMonster(*equipped)) *equipped=0;
    return changed;
}
