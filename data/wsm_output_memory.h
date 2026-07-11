#ifndef WSM_OUTPUT_MEMORY_H
#define WSM_OUTPUT_MEMORY_H

#include <stdbool.h>

struct output_config;
struct wsm_output;

/** The returned string must be freed by the caller. */
char *wsm_output_memory_get_primary_output(void);
bool wsm_output_memory_set_primary_output(struct wsm_output *output);

bool wsm_output_memory_load(
	struct wsm_output *output, struct output_config *config);
void wsm_output_memory_store_all(void);
void wsm_output_memory_finish(void);

#endif
