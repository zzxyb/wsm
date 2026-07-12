#ifndef WSM_TASK_VIEW_H
#define WSM_TASK_VIEW_H

#include <stdbool.h>
#include <stddef.h>

struct wsm_task_model;

struct wsm_task_view_impl {
	bool (*show)(void *data, const struct wsm_task_model *model,
		size_t selected);
	void (*update)(void *data, const struct wsm_task_model *model,
		size_t selected);
	void (*hide)(void *data);
};

struct wsm_task_view {
	const struct wsm_task_view_impl *impl;
	void *data;
};

#endif
