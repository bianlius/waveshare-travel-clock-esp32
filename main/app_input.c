#include "app_input.h"
#include "config.h"
#include "GVIEW/gview.h"
#include "sound.h"
#include "PAGE/pageDraw.h"

extern void doKeyFun(KeyName);

int appInputAction(int button, bool held){
    if (button < 0 || button > 2) return -1;
    if (held) return button == 0 ? 3 : button == 2 ? 4 : -1;
    if (softKeyHeaderVisible && button != 1) return pageUIKeyAction(button);
    return button == 0 ? 2 : button == 2 ? 0 : 1;
}

bool appInputBegin(void){
    backHomeTime = 120;
    if (!aClockTimes) return true;
    aClockTimes = 0;
    soundStopAll();
    hideTips();
    return false;
}

void appInputDispatch(int button, bool held){
    if (button < 0 || button > 2 || !appInputBegin()) return;
    if (button == 1 && held) {
        renderPowerMenuInit();
        return;
    }
    static const KeyName keys[] = {KM1, KM2, KM3, KML, KMR};
    const int action = appInputAction(button, held);
    if (action >= 0 && action < 5) doKeyFun(keys[action]);
}
