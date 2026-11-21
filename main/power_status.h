#ifndef GVIEW_POWER_STATUS_H
#define GVIEW_POWER_STATUS_H

// Board-reported state; USB host presence does not imply battery charging.
typedef enum {
    POWER_STATUS_NONE,
    POWER_STATUS_CHARGING,
    POWER_STATUS_USB_POWERED
} PowerStatus;

PowerStatus powerStatusGet(void);
#endif
