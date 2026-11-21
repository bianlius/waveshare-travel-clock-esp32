#ifndef SHOP_MODEL_H
#define SHOP_MODEL_H
#include <stdint.h>
// Retired saved IDs are never reassigned to another species.
enum { SHOP_RETIRED_ZIGZAGOON=12, SHOP_RETIRED_BRELOOM=14, SHOP_RETIRED_VOLBEAT=17,
       SHOP_RETIRED_MEOWTH=20, SHOP_RETIRED_LUGIA=21, SHOP_RETIRED_HO_OH=22,
       SHOP_RETIRED_PIDGEOT=23, SHOP_RETIRED_REGICE=38, SHOP_RETIRED_LUCARIO=45,
       SHOP_FIRST_ADDED_FOLLOWER=10,
       SHOP_FIRST_FLYING_FOLLOWER=15,
       SHOP_FIRST_DOWNLOADED_FOLLOWER=28, SHOP_MONSTER_CAPACITY=128 };
uint8_t shopRemoveRetiredMonster(uint64_t *owned,uint8_t *equipped);
uint8_t shopIsRetiredMonster(uint8_t item);
typedef enum { SHOP_UNLOCKED, SHOP_EQUIPPED, SHOP_ALREADY_OWNED, SHOP_NOT_ENOUGH, SHOP_INVALID } ShopActionResult;
ShopActionResult shopApply(uint64_t *owned, uint8_t *equipped, uint8_t item, uint16_t cost, uint32_t *balance);
uint8_t shopHasItem(uint64_t owned, uint64_t extra, uint8_t item);
ShopActionResult shopApplyExtended(uint64_t *owned, uint64_t *extra, uint8_t *equipped,
                                   uint8_t item, uint16_t cost, uint32_t *balance);
void shopBuildPriceOrder(const uint16_t *costs, uint8_t count, uint8_t *order);
void shopBuildMonsterOrder(const uint16_t *costs, uint8_t count, uint8_t *order);
#endif
