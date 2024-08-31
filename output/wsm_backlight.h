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

#ifndef WSM_BACKLIGHT_H
#define WSM_BACKLIGHT_H

#include <stdbool.h>

struct wsm_output;

/**
 * @brief backlight device type
 *
 * @details identify and classify different backlight control devices
 * so that they can be processed and optimized accordingly.ref type in
 * the /sys/class/backlight directory
 */
enum wsm_backlight_type {
	/**< default value. */
	WSM_BACKLIGHT_UNKNOW,
	/**< low-level interface for direct control of backlight hardware. */
	WSM_BACKLIGHT_RAM,
	/**< the backlight control interface related to the specific hardware
	 * platform may provide higher-level control through some abstraction layers. */
	WSM_BACKLIGHT_PLATFORM,
	/**< backlighting controlled by system firmware (such as BIOS or UEFI). */
	WSM_BACKLIGHT_FIRMWARE,
	/**< specific backlight control devices. Such devices are usually provided
	 * by the hardware manufacturer and may include some customized control mechanisms. */
	WSM_BACKLIGHT_VENDOR,
};

struct wsm_backlight {
	struct wsm_output* wsm_output;
	int max_brightness;
	int brightness;
	enum wsm_backlight_type type;
};

struct wsm_backlight *wsm_backlight_create(struct wsm_output *output);
void wsm_backlight_destroy(struct wsm_backlight *backlight);
int wsm_backlight_get_max_brightness(struct wsm_backlight *backlight);
int wsm_backlight_get_brightness(struct wsm_backlight *backlight);
int wsm_backlight_get_actual_brightness(struct wsm_backlight *backlight);
bool wsm_backlight_set_brightness(struct wsm_backlight *backlight, long brightness);

#endif
