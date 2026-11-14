#ifndef SHOP_MODEL_H
#define SHOP_MODEL_H
#include <stdint.h>
// Retired saved ID is never reassigned to another species.
enum { SHOP_RETIRED_BRELOOM=14, SHOP_FIRST_FLYING_FOLLOWER=15 };
uint8_t shopRemoveRetiredMonster(uint64_t *owned,uint8_t *equipped);
typedef enum { SHOP_UNLOCKED, SHOP_EQUIPPED, SHOP_ALREADY_OWNED, SHOP_NOT_ENOUGH, SHOP_INVALID } ShopActionResult;
ShopActionResult shopApply(uint64_t *owned, uint8_t *equipped, uint8_t item, uint16_t cost, uint32_t *balance);
void shopBuildPriceOrder(const uint16_t *costs, uint8_t count, uint8_t *order);
#endif
