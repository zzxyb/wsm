#ifndef WSM_INPUT_MEMORY_H
#define WSM_INPUT_MEMORY_H

#include <stdbool.h>

struct wsm_input_device;

bool wsm_input_memory_configure(struct wsm_input_device *device);
int wsm_input_memory_get_repeat_rate(void);
int wsm_input_memory_get_repeat_delay(void);
bool wsm_input_memory_get_numlock(void);
bool wsm_input_memory_set_repeat_info(int rate, int delay);
bool wsm_input_memory_set_numlock(bool enabled);
void wsm_input_memory_finish(void);

#endif
