/*
 * Copyright © 2024 YaoBing Xiao
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WSM_TITLEBAR_H
#define WSM_TITLEBAR_H

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_scene_rect;
struct wlr_scene_tree;

struct wsm_text_node;
struct wsm_image_node;
struct wsm_image_button;

/**
 * @brief Enumeration of titlebar states in the WSM
 */
enum wsm_titlebar_state {
	WSM_ACTIVE = 0, /**< Titlebar is active */
	WSM_INACTIVE, /**< Titlebar is inactive */
	WSM_FOCUS_IN, /**< Titlebar has focus */
	WSM_FOCUS_OUT, /**< Titlebar lost focus */
};

/**
 * @brief Enumeration of parts in the server decoration
 */
enum ssd_part_type {
	SSD_NONE = 0, /**< No part */
	SSD_BUTTON_CLOSE, /**< Close button part */
	SSD_BUTTON_MAXIMIZE, /**< Maximize button part */
	SSD_BUTTON_MINIMIZE, /**< Minimize button part */
	SSD_BUTTON_ICONIFY, /**< Iconify button part */
	SSD_PART_TITLEBAR, /**< Titlebar part */
	SSD_PART_TITLE, /**< Title part */
	SSD_PART_TOP_LEFT, /**< Top left part */
	SSD_PART_TOP_RIGHT, /**< Top right part */
	SSD_PART_BOTTOM_RIGHT, /**< Bottom right part */
	SSD_PART_BOTTOM_LEFT, /**< Bottom left part */
	SSD_PART_TOP, /**< Top part */
	SSD_PART_RIGHT, /**< Right part */
	SSD_PART_BOTTOM, /**< Bottom part */
	SSD_PART_LEFT, /**< Left part */
	SSD_CLIENT, /**< Client part */
	SSD_FRAME, /**< Frame part */
	SSD_ROOT, /**< Root part */
	SSD_MENU, /**< Menu part */
	SSD_OSD, /**< On-screen display part */
	SSD_LAYER_SURFACE, /**< Layer surface part */
	SSD_LAYER_SUBSURFACE, /**< Layer subsurface part */
	SSD_UNMANAGED, /**< Unmanaged part */
	SSD_END_MARKER /**< End marker for parts */
};

/**
 * @brief Structure representing a titlebar in the WSM
 */
struct wsm_titlebar {
	struct {
		struct wl_signal double_click; /**< Signal for double-click events */
		struct wl_signal request_state; /**< Signal for state request events */
	} events;

	struct wlr_scene_tree *tree; /**< Pointer to the scene tree for the titlebar */

	struct wlr_scene_rect *background; /**< Pointer to the background rectangle */
	struct wsm_image_node *icon; /**< Pointer to the icon node */
	struct wsm_text_node *title_text; /**< Pointer to the title text node */

	struct wsm_image_button *min_button; /**< Pointer to the minimize button */
	struct wsm_image_button *max_button; /**< Pointer to the maximize button */
	struct wsm_image_button *close_button; /**< Pointer to the close button */
	bool active; /**< Flag indicating if the titlebar is active */
};

/**
 * @brief Structure representing a request to change the state of a titlebar
 */
struct wsm_titlebar_event_request_state {
	struct wsm_titlebar *titlebar; /**< Pointer to the associated titlebar */
	enum wsm_titlebar_state state; /**< Desired state for the titlebar */
};

/**
 * @brief Creates a new wsm_titlebar instance
 * @return Pointer to the newly created wsm_titlebar instance
 */
struct wsm_titlebar* wsm_titlebar_create();

/**
 * @brief Destroys the specified wsm_titlebar instance
 * @param titlebar Pointer to the wsm_titlebar instance to destroy
 */
void wsm_titlebar_destroy(struct wsm_titlebar *titlebar);

#endif
