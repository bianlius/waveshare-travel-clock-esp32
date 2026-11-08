#ifndef SHOP_MODEL_H
#define SHOP_MODEL_H
#include <stdint.h>
typedef enum { SHOP_UNLOCKED, SHOP_EQUIPPED, SHOP_ALREADY_OWNED, SHOP_NOT_ENOUGH, SHOP_INVALID } ShopActionResult;
ShopActionResult shopApply(uint64_t *owned, uint8_t *equipped, uint8_t item, uint16_t cost, uint32_t *balance);
#endif
