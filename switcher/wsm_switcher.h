#ifndef WSM_SWITCHER_H
#define WSM_SWITCHER_H

#include "wsm_task_session.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wlr_scene_rect;
struct wlr_scene_tree;
struct wsm_seat;
struct wsm_image_node;
struct wsm_text_node;

struct wsm_switcher {
	struct wsm_task_session session;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
	struct wlr_scene_rect *selection;
	struct wsm_image_node **icons;
	size_t icons_length;
	struct wsm_text_node *app_name;
	bool tab_down;
	bool escape_down;
};

struct wsm_switcher *wsm_switcher_create(struct wsm_seat *seat);
void wsm_switcher_destroy(struct wsm_switcher *switcher);
bool wsm_switcher_handle_key(struct wsm_switcher *switcher,
	const uint32_t *keysyms, size_t keysyms_len, uint32_t modifiers,
	uint32_t state);
void wsm_switcher_handle_modifiers(
	struct wsm_switcher *switcher, uint32_t modifiers);

#endif
