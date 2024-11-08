#ifndef WSM_TEXT_NODE_H
#define WSM_TEXT_NODE_H

#include <stdbool.h>

struct wlr_scene_node;
struct wlr_scene_tree;

struct wsm_desktop_interface;

/**
 * @brief Structure representing a text node in the WSM
 */
struct wsm_text_node {
	float color[4]; /**< RGBA color of the text */
	float background[4]; /**< RGBA background color of the text node */

	struct wlr_scene_node *node_wlr; /**< Pointer to the associated WLR scene node */
	int width; /**< Width of the text node */
	int max_width; /**< Maximum width of the text node */
	int height; /**< Height of the text node */
	int baseline; /**< Baseline position for the text */
	bool pango_markup; /**< Flag indicating if Pango markup is used for text formatting */
};

/**
 * @brief Creates a new wsm_text_node instance
 * @param parent Pointer to the parent scene tree where the text node will be created
 * @param font Pointer to the desktop interface providing font information
 * @param text Pointer to the text string to be displayed
 * @param color RGBA color array for the text
 * @param pango_markup Boolean indicating if Pango markup should be used
 * @return Pointer to the newly created wsm_text_node instance
 */
struct wsm_text_node *wsm_text_node_create(struct wlr_scene_tree *parent,
	const struct wsm_desktop_interface *font,
	char *text, float color[4], bool pango_markup);

/**
 * @brief Sets the color of the specified text node
 * @param node Pointer to the wsm_text_node whose color will be set
 * @param color RGBA color array to set for the text
 */
void wsm_text_node_set_color(struct wsm_text_node *node, float color[4]);

/**
 * @brief Sets the text of the specified text node
 * @param node Pointer to the wsm_text_node whose text will be set
 * @param text Pointer to the new text string to be displayed
 */
void wsm_text_node_set_text(struct wsm_text_node *node, char *text);

/**
 * @brief Sets the maximum width of the specified text node
 * @param node Pointer to the wsm_text_node whose maximum width will be set
 * @param max_width Maximum width to set for the text node
 */
void wsm_text_node_set_max_width(struct wsm_text_node *node, int max_width);

/**
 * @brief Sets the background color of the specified text node
 * @param node Pointer to the wsm_text_node whose background color will be set
 * @param background RGBA color array to set for the background
 */
void wsm_text_node_set_background(struct wsm_text_node *node, float background[4]);

#endif
