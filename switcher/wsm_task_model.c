#include "wsm_task_model.h"

#include "wsm_seat.h"
#include "wsm_container.h"
#include "wsm_desktop.h"
#include "wsm_view.h"
#include "node/wsm_node.h"

#include <stdlib.h>
#include <string.h>

static void handle_item_destroy(struct wl_listener *listener, void *data) {
	struct wsm_task_item *item = wl_container_of(listener, item, destroy);
	wl_list_remove(&item->destroy.link);
	wl_list_init(&item->destroy.link);
	item->node = NULL;
}

void wsm_task_model_init(struct wsm_task_model *model) {
	*model = (struct wsm_task_model){0};
}

void wsm_task_model_finish(struct wsm_task_model *model) {
	for (size_t i = 0; i < model->length; ++i) {
		if (model->items[i].destroy.link.next != NULL) {
			wl_list_remove(&model->items[i].destroy.link);
		}
		free(model->items[i].app_name);
		free(model->items[i].icon_path);
	}
	free(model->items);
	*model = (struct wsm_task_model){0};
}

static void model_add(
		struct wsm_task_model *model, struct wsm_node *node) {
	struct wsm_task_item *item = &model->items[model->length++];
	*item = (struct wsm_task_item){.node = node};
	struct wsm_view *view = node->container->view;
	const char *app_id = view_get_app_id(view);
	const char *class = view_get_class(view);
	const char *title = view_get_title(view);
	item->app_name = find_app_name_from_app_id(app_id);
	if (item->app_name == NULL && class != NULL) {
		item->app_name = find_app_name_from_app_id(class);
	}
	if (item->app_name == NULL) {
		const char *fallback = class != NULL ? class
			: app_id != NULL ? app_id : title;
		item->app_name = strdup(fallback != NULL ? fallback : "Unknown");
	}
	if (view->app_icon_path != NULL) {
		item->icon_path = strdup(view->app_icon_path);
	}
	wl_list_init(&item->destroy.link);
	item->destroy.notify = handle_item_destroy;
	wl_signal_add(&node->events.destroy, &item->destroy);
}

bool wsm_task_model_collect_mru(
		struct wsm_task_model *model, struct wsm_seat *seat) {
	wsm_task_model_finish(model);
	struct wsm_seat_node *seat_node;
	size_t count = 0;
	wl_list_for_each(seat_node, &seat->focus_stack, link) {
		if (node_is_view(seat_node->node) && !seat_node->node->destroying) {
			count++;
		}
	}
	if (count > 0) {
		model->items = calloc(count, sizeof(*model->items));
		if (model->items == NULL) {
			return false;
		}
	}
	model->capacity = count;
	wl_list_for_each(seat_node, &seat->focus_stack, link) {
		struct wsm_node *node = seat_node->node;
		if (!node_is_view(node) || node->destroying) {
			continue;
		}
		model_add(model, node);
	}
	return true;
}

struct wsm_node *wsm_task_model_get(
		const struct wsm_task_model *model, size_t index) {
	return index < model->length ? model->items[index].node : NULL;
}

const char *wsm_task_model_get_app_name(
		const struct wsm_task_model *model, size_t index) {
	return index < model->length ? model->items[index].app_name : NULL;
}

const char *wsm_task_model_get_icon_path(
		const struct wsm_task_model *model, size_t index) {
	return index < model->length ? model->items[index].icon_path : NULL;
}
