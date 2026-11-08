#include "shop_model.h"
#include <stddef.h>
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
