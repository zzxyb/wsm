#ifndef WSM_GESTURE_H
#define WSM_GESTURE_H

#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_pointer.h>

enum wsm_event_type {
    WSM_EVENT_NOTHING = 0,
    WSM_EVENT_BUTTON_PRESS = 1,
    WSM_EVENT_BUTTON_RELEASE = 2,
    WSM_EVENT_MOTION_NOTIFY = 3,

    WSM_EVENT_TOUCH_BEGIN = 4,
    WSM_EVENT_TOUCH_UPDATE = 5,
    WSM_EVENT_TOUCH_END = 6,
};

#endif
