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

enum wsm_gesture_type {
	GESTURE_TYPE_NONE = 0,
	GESTURE_TYPE_HOLD,
	GESTURE_TYPE_PINCH,
	GESTURE_TYPE_SWIPE,
};

struct wsm_gesture_tracker {
	enum wsm_gesture_type type;
	uint8_t fingers;
	double dx, dy;
	double scale;
	double rotation;
};

#endif
