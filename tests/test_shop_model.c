#include "../main/PAGE/SHOP/shop_model.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    const uint16_t costs[]={0,200,500,150,150,300};
    uint8_t order[6];
    const uint8_t expected[]={0,3,4,1,5,2};
    shopBuildPriceOrder(costs,6,order);
    for(unsigned i=0;i<6;++i) assert(order[i]==expected[i]);
    const uint16_t monsterCosts[]={0,200,200,200,500,500,200,1000,800,1800,400,100,100,1600,0,3000};
    const uint8_t monsterExpected[]={0,1,2,3,4,5,6,7,8,9,14,11,12,10,13,15};
    uint8_t monsterOrder[16];
    shopBuildMonsterOrder(monsterCosts,16,monsterOrder);
    for(unsigned i=0;i<16;++i)assert(monsterOrder[i]==monsterExpected[i]);
    for(unsigned count=0;count<=10;++count){
        shopBuildMonsterOrder(monsterCosts,count,monsterOrder);
        for(unsigned i=0;i<count;++i)assert(monsterOrder[i]==i);
    }
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
    uint64_t extra=0;
    const uint64_t previous=owned;
    for(uint8_t id=63;id<=65;++id){
        money=125;
        assert(shopApplyExtended(&owned,&extra,&equipped,id,100,&money)==SHOP_UNLOCKED);
        assert(money==25 && (owned&previous)==previous);
        assert(shopHasItem(owned,extra,id));
        assert(shopApplyExtended(&owned,&extra,&equipped,id,100,&money)==SHOP_EQUIPPED);
        assert(equipped==id && money==25);
        assert(shopApplyExtended(&owned,&extra,&equipped,id,100,&money)==SHOP_ALREADY_OWNED);
    }
    assert(extra==3 && (owned&(UINT64_C(1)<<63)));
    money=0;
    assert(shopApplyExtended(&owned,&extra,&equipped,127,1,&money)==SHOP_NOT_ENOUGH);
    assert(shopApplyExtended(&owned,&extra,&equipped,127,0,&money)==SHOP_UNLOCKED);
    assert(extra==(3|(UINT64_C(1)<<63)));
    assert(shopApplyExtended(&owned,&extra,&equipped,128,0,&money)==SHOP_INVALID);
    assert(!shopHasItem(owned,extra,128));
    assert(!shopHasItem(owned,extra,255));
    // Retired Pokemon IDs are migrated without disturbing adjacent/high IDs.
    const uint8_t retiredIds[]={12,14,17,20,21,22,23,38,45};
    uint64_t retiredMask=0;
    for(unsigned i=0;i<sizeof(retiredIds);++i) retiredMask|=UINT64_C(1)<<retiredIds[i];
    const uint64_t keep=~retiredMask;
    for(unsigned item=0;item<256;++item)
        assert(shopIsRetiredMonster(item)==(item<64 && (retiredMask&(UINT64_C(1)<<item))!=0));
    for(unsigned which=0;which<sizeof(retiredIds);++which){
        equipped=retiredIds[which];
        owned=keep|retiredMask;
        assert(shopIsRetiredMonster(equipped));
        assert(shopRemoveRetiredMonster(&owned,&equipped));
        assert(equipped==0 && owned==keep);
        assert(!shopRemoveRetiredMonster(&owned,&equipped));
        // A stale selection is reset even when its ownership bit is absent.
        equipped=retiredIds[which];owned=keep;
        assert(shopRemoveRetiredMonster(&owned,&equipped));
        assert(equipped==0 && owned==keep);
    }
    equipped=46;owned=keep|(UINT64_C(1)<<45);
    assert(shopRemoveRetiredMonster(&owned,&equipped));
    assert(owned==keep && equipped==46 && !shopIsRetiredMonster(46));
    // Removed Regice cannot remain equipped or retain ownership at ID 38.
    equipped=38;owned=(UINT64_C(1)<<38)|(UINT64_C(1)<<45);
    assert(shopRemoveRetiredMonster(&owned,&equipped));
    assert(owned==0 && equipped==0 && shopIsRetiredMonster(38));
    // Purchase helpers also serve garden items: item 14 remains valid there.
    owned=0;money=100;
    assert(shopApply(&owned,NULL,14,100,&money)==SHOP_UNLOCKED);
    assert(money==0 && owned==(UINT64_C(1)<<14));
    puts("shop purchase/equip checks passed");
}
