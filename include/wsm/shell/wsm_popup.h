#ifndef SHELL_WSM_POPUP_H
#define SHELL_WSM_POPUP_H

struct wsm_window;
struct wlr_scene_nod;

struct wsm_popup_description {
	struct wlr_scene_node *relative;
	struct wsm_window *window;
};

#endif
