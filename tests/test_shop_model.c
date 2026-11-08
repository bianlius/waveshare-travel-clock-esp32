#include "../main/PAGE/SHOP/shop_model.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint64_t owned=1;
    uint8_t equipped=0;
    uint32_t money=200;
    assert(shopApply(&owned,&equipped,1,200,&money)==SHOP_UNLOCKED);
    assert(money==0 && owned==3 && equipped==0);
    assert(shopApply(&owned,&equipped,1,200,&money)==SHOP_EQUIPPED);
    assert(equipped==1 && money==0);
    assert(shopApply(&owned,&equipped,1,200,&money)==SHOP_ALREADY_OWNED);
    assert(money==0 && owned==3);
    assert(shopApply(&owned,&equipped,2,1,&money)==SHOP_NOT_ENOUGH);
    assert(equipped==1 && owned==3 && money==0);
    assert(shopApply(&owned,&equipped,0,0,&money)==SHOP_EQUIPPED);
    assert(equipped==0 && owned==3);
    money=99;
    assert(shopApply(&owned,0,40,100,&money)==SHOP_NOT_ENOUGH);
    money=100;
    assert(shopApply(&owned,0,40,100,&money)==SHOP_UNLOCKED);
    assert((owned&(UINT64_C(1)<<40)) && money==0);
    assert(shopApply(&owned,0,40,100,&money)==SHOP_ALREADY_OWNED);
    assert(shopApply(&owned,&equipped,64,0,&money)==SHOP_INVALID);
    puts("shop purchase/equip checks passed");
}
