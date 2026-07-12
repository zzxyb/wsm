#ifndef WSM_TASK_MODEL_H
#define WSM_TASK_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <wayland-server-core.h>

struct wsm_node;
struct wsm_seat;

struct wsm_task_item {
	struct wsm_node *node;
	char *app_name;
	char *icon_path;
	struct wl_listener destroy;
};

struct wsm_task_model {
	struct wsm_task_item *items;
	size_t length;
	size_t capacity;
};

void wsm_task_model_init(struct wsm_task_model *model);
void wsm_task_model_finish(struct wsm_task_model *model);
bool wsm_task_model_collect_mru(
	struct wsm_task_model *model, struct wsm_seat *seat);
struct wsm_node *wsm_task_model_get(
	const struct wsm_task_model *model, size_t index);
const char *wsm_task_model_get_app_name(
	const struct wsm_task_model *model, size_t index);
const char *wsm_task_model_get_icon_path(
	const struct wsm_task_model *model, size_t index);

#endif
