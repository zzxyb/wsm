#include "wsm_switcher.h"

#include "wsm_scene.h"
#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_desktop.h"
#include "node/wsm_image_node.h"
#include "node/wsm_text_node.h"

#include <stdlib.h>

#include <wayland-server-protocol.h>

#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#define ICON_SIZE 96
#define ICON_SLOT_SIZE 120
#define ICON_GAP 12
#define PANEL_PADDING 24
#define LABEL_GAP 12

static void clear_content(struct wsm_switcher *switcher) {
	for (size_t i = 0; i < switcher->icons_length; ++i) {
		if (switcher->icons[i] != NULL) {
			wlr_scene_node_destroy(switcher->icons[i]->node_wlr);
		}
	}
	free(switcher->icons);
	switcher->icons = NULL;
	switcher->icons_length = 0;
	if (switcher->selection != NULL) {
		wlr_scene_node_destroy(&switcher->selection->node);
		switcher->selection = NULL;
	}
	if (switcher->app_name != NULL) {
		wlr_scene_node_destroy(switcher->app_name->node_wlr);
		switcher->app_name = NULL;
	}
}

static bool layout_cards(struct wsm_switcher *switcher,
		const struct wsm_task_model *model, size_t selected) {
	clear_content(switcher);
	struct wlr_box root;
	root_get_box(global_server.scene, &root);
	int available = root.width - PANEL_PADDING * 2;
	size_t visible = available > ICON_SLOT_SIZE
		? (size_t)(available + ICON_GAP) / (ICON_SLOT_SIZE + ICON_GAP)
		: 1;
	if (visible > model->length) {
		visible = model->length;
	}
	size_t first = selected >= visible ? selected - visible + 1 : 0;
	if (first + visible > model->length) {
		first = model->length - visible;
	}
	int label_height = global_server.desktop_interface->font_height;
	int width = PANEL_PADDING * 2 + (int)visible * ICON_SLOT_SIZE +
		(int)(visible > 0 ? visible - 1 : 0) * ICON_GAP;
	int height = PANEL_PADDING * 2 + ICON_SLOT_SIZE + LABEL_GAP + label_height;
	int x = root.x + (root.width - width) / 2;
	int y = root.y + (root.height - height) / 2;
	wlr_scene_node_set_position(&switcher->tree->node, x, y);
	wlr_scene_rect_set_size(switcher->background, width, height);

	float selection_color[4] = {0.32f, 0.32f, 0.35f, 0.78f};
	switcher->selection = wlr_scene_rect_create(switcher->tree,
		ICON_SLOT_SIZE, ICON_SLOT_SIZE, selection_color);
	if (switcher->selection == NULL) {
		return false;
	}
	switcher->icons = calloc(visible, sizeof(*switcher->icons));
	if (switcher->icons == NULL && visible > 0) {
		clear_content(switcher);
		return false;
	}
	switcher->icons_length = visible;
	int selected_slot_x = PANEL_PADDING;
	for (size_t i = 0; i < visible; ++i) {
		size_t index = first + i;
		int slot_x = PANEL_PADDING + (int)i * (ICON_SLOT_SIZE + ICON_GAP);
		if (index == selected) {
			selected_slot_x = slot_x;
			wlr_scene_node_set_position(
				&switcher->selection->node, slot_x, PANEL_PADDING);
		}
		const char *icon_path =
			wsm_task_model_get_icon_path(model, index);
		if (icon_path == NULL || *icon_path == '\0') {
			continue;
		}
		switcher->icons[i] = wsm_image_node_create(switcher->tree,
			ICON_SIZE, ICON_SIZE, (char *)icon_path, 1.0f);
		if (switcher->icons[i] != NULL) {
			wlr_scene_node_set_position(switcher->icons[i]->node_wlr,
				slot_x + (ICON_SLOT_SIZE - ICON_SIZE) / 2,
				PANEL_PADDING + (ICON_SLOT_SIZE - ICON_SIZE) / 2);
		}
	}
	const char *name = wsm_task_model_get_app_name(model, selected);
	float text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	switcher->app_name = wsm_text_node_create(switcher->tree,
		global_server.desktop_interface, (char *)(name != NULL ? name : ""),
		text_color, false);
	if (switcher->app_name != NULL) {
		int max_text_width = ICON_SLOT_SIZE;
		wsm_text_node_set_max_width(switcher->app_name, max_text_width);
		wsm_text_node_set_ellipsize(switcher->app_name, true);
		int text_width = switcher->app_name->width < max_text_width
			? switcher->app_name->width : max_text_width;
		wlr_scene_node_set_position(switcher->app_name->node_wlr,
			selected_slot_x + (ICON_SLOT_SIZE - text_width) / 2,
			PANEL_PADDING + ICON_SLOT_SIZE + LABEL_GAP);
	}
	return true;
}

static bool view_show(void *data, const struct wsm_task_model *model,
		size_t selected) {
	struct wsm_switcher *switcher = data;
	if (!layout_cards(switcher, model, selected)) {
		return false;
	}
	wlr_scene_node_set_enabled(&switcher->tree->node, true);
	wlr_scene_node_raise_to_top(&switcher->tree->node);
	return true;
}

static void view_update(void *data, const struct wsm_task_model *model,
		size_t selected) {
	layout_cards(data, model, selected);
}

static void view_hide(void *data) {
	struct wsm_switcher *switcher = data;
	wlr_scene_node_set_enabled(&switcher->tree->node, false);
	clear_content(switcher);
}

static const struct wsm_task_view_impl switcher_view_impl = {
	.show = view_show,
	.update = view_update,
	.hide = view_hide,
};

struct wsm_switcher *wsm_switcher_create(struct wsm_seat *seat) {
	struct wsm_switcher *switcher = calloc(1, sizeof(*switcher));
	if (switcher == NULL) {
		return NULL;
	}
	switcher->tree = wlr_scene_tree_create(seat->scene_tree);
	if (switcher->tree == NULL) {
		free(switcher);
		return NULL;
	}
	float background[4] = {0.06f, 0.06f, 0.075f, 1.0f};
	switcher->background =
		wlr_scene_rect_create(switcher->tree, 0, 0, background);
	if (switcher->background == NULL) {
		wlr_scene_node_destroy(&switcher->tree->node);
		free(switcher);
		return NULL;
	}
	wlr_scene_node_set_enabled(&switcher->tree->node, false);
	struct wsm_task_view view = {
		.impl = &switcher_view_impl,
		.data = switcher,
	};
	wsm_task_session_init(&switcher->session, seat, &view);
	return switcher;
}

void wsm_switcher_destroy(struct wsm_switcher *switcher) {
	if (switcher == NULL) {
		return;
	}
	wsm_task_session_finish(&switcher->session);
	clear_content(switcher);
	wlr_scene_node_destroy(&switcher->tree->node);
	free(switcher);
}

static bool has_keysym(
		const uint32_t *keysyms, size_t length, uint32_t wanted) {
	for (size_t i = 0; i < length; ++i) {
		if (keysyms[i] == wanted) {
			return true;
		}
	}
	return false;
}

bool wsm_switcher_handle_key(struct wsm_switcher *switcher,
		const uint32_t *keysyms, size_t keysyms_len, uint32_t modifiers,
		uint32_t state) {
	if (switcher == NULL) {
		return false;
	}
	bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
	bool tab = has_keysym(keysyms, keysyms_len, XKB_KEY_Tab) ||
		has_keysym(keysyms, keysyms_len, XKB_KEY_ISO_Left_Tab);
	if (!switcher->session.active) {
		if (!pressed && tab && switcher->tab_down) {
			switcher->tab_down = false;
			return true;
		}
		if (!pressed && has_keysym(keysyms, keysyms_len, XKB_KEY_Escape) &&
				switcher->escape_down) {
			switcher->escape_down = false;
			return true;
		}
		if (!pressed || !tab || !(modifiers & WLR_MODIFIER_ALT)) {
			return false;
		}
		switcher->tab_down = true;
		return wsm_task_session_begin(&switcher->session,
			(modifiers & WLR_MODIFIER_SHIFT) != 0);
	}
	if (pressed && tab) {
		switcher->tab_down = true;
		wsm_task_session_step(&switcher->session,
			(modifiers & WLR_MODIFIER_SHIFT) != 0);
		return true;
	}
	if (pressed && has_keysym(keysyms, keysyms_len, XKB_KEY_Escape)) {
		switcher->escape_down = true;
		wsm_task_session_cancel(&switcher->session);
		return true;
	}
	return tab || has_keysym(keysyms, keysyms_len, XKB_KEY_Escape);
}

void wsm_switcher_handle_modifiers(
		struct wsm_switcher *switcher, uint32_t modifiers) {
	if (switcher != NULL && switcher->session.active &&
			!(modifiers & WLR_MODIFIER_ALT)) {
		wsm_task_session_commit(&switcher->session);
	}
}
