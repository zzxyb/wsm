/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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
