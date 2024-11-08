#ifndef WSM_ARRANGE_H
#define WSM_ARRANGE_H

#include "wsm_container.h"

struct wsm_list;
struct wsm_scene;
struct wsm_output;
struct wsm_workspace;

/**
 * @brief Automatically arranges the root window
 */
void arrange_root_auto(void);

/**
 * @brief Arranges the specified root scene
 * @param root Pointer to the wsm_scene to be arranged
 */
void arrange_root_scene(struct wsm_scene *root);

/**
 * @brief Automatically arranges the specified output
 * @param output Pointer to the wsm_output to be arranged
 */
void wsm_arrange_output_auto(struct wsm_output *output);

/**
 * @brief Arranges the output with the specified width and height
 * @param output Pointer to the wsm_output to be arranged
 * @param width Desired width for the output
 * @param height Desired height for the output
 */
void arrange_output_width_size(struct wsm_output *output, int width, int height);

/**
 * @brief Automatically arranges the specified workspace
 * @param workspace Pointer to the wsm_workspace to be arranged
 */
void wsm_arrange_workspace_auto(struct wsm_workspace *workspace);

/**
 * @brief Arranges the workspace in a tiling layout
 * @param ws Pointer to the wsm_workspace to be arranged
 * @param width Desired width for the workspace
 * @param height Desired height for the workspace
 */
void arrange_workspace_tiling(struct wsm_workspace *ws, int width, int height);

/**
 * @brief Arranges the workspace in a floating layout
 * @param ws Pointer to the wsm_workspace to be arranged
 */
void arrange_workspace_floating(struct wsm_workspace *ws);

/**
 * @brief Arranges a layer surface within the specified output
 * @param output Pointer to the wsm_output where the layer surface will be arranged
 * @param full_area Pointer to the full area of the output
 * @param usable_area Pointer to the usable area of the output
 * @param tree Pointer to the scene tree containing the layer surface
 */
void wsm_arrange_layer_surface(struct wsm_output *output, const struct wlr_box *full_area,
	struct wlr_box *usable_area, struct wlr_scene_tree *tree);

/**
 * @brief Arranges the specified popups within the scene tree
 * @param popups Pointer to the scene tree containing the popups
 */
void wsm_arrange_popups(struct wlr_scene_tree *popups);

/**
 * @brief Arranges the layers within the specified output
 * @param output Pointer to the wsm_output where the layers will be arranged
 */
void wsm_arrange_layers(struct wsm_output *output);

/**
 * @brief Automatically arranges the specified container
 * @param container Pointer to the wsm_container to be arranged
 */
void wsm_arrange_container_auto(struct wsm_container *container);

/**
 * @brief Arranges the specified container with a title bar
 * @param con Pointer to the wsm_container to be arranged
 * @param width Desired width for the container
 * @param height Desired height for the container
 * @param title_bar Boolean indicating if a title bar should be included
 * @param gaps Gaps to be applied around the container
 */
void wsm_arrange_container_with_title_bar(struct wsm_container *con,
	int width, int height, bool title_bar, int gaps);

/**
 * @brief Arranges the children of a container
 * @param children Pointer to the list of children to be arranged
 * @param layout Layout type for arranging the children
 * @param parent Pointer to the parent box for the arrangement
 */
void wsm_arrange_children(struct wsm_list *children,
	enum wsm_container_layout layout, struct wlr_box *parent);

/**
 * @brief Arranges children with a title bar
 * @param layout Layout type for arranging the children
 * @param children Pointer to the list of children to be arranged
 * @param active Pointer to the active container
 * @param content Pointer to the scene tree containing the content
 * @param width Desired width for the arrangement
 * @param height Desired height for the arrangement
 * @param gaps Gaps to be applied around the arrangement
 */
void arrange_children_with_titlebar(enum wsm_container_layout layout, struct wsm_list *children,
	struct wsm_container *active, struct wlr_scene_tree *content,
	int width, int height, int gaps);

/**
 * @brief Arranges the specified floating windows
 * @param floating Pointer to the list of floating windows to be arranged
 */
void wsm_arrange_floating(struct wsm_list *floating);

/**
 * @brief Arranges the title bar node of the specified container
 * @param con Pointer to the wsm_container whose title bar node will be arranged
 */
void container_arrange_title_bar_node(struct wsm_container *con);

/**
 * @brief Arranges the title bar of the specified container
 * @param con Pointer to the wsm_container whose title bar will be arranged
 * @param x X coordinate for the title bar
 * @param y Y coordinate for the title bar
 * @param width Desired width for the title bar
 * @param height Desired height for the title bar
 */
void wsm_arrange_title_bar(struct wsm_container *con,
	int x, int y, int width, int height);

/**
 * @brief Arranges the specified container in fullscreen mode
 * @param tree Pointer to the scene tree containing the fullscreen container
 * @param fs Pointer to the wsm_container to be arranged in fullscreen
 * @param ws Pointer to the wsm_workspace where the fullscreen container resides
 * @param width Desired width for the fullscreen arrangement
 * @param height Desired height for the fullscreen arrangement
 */
void wsm_arrange_fullscreen(struct wlr_scene_tree *tree,
	struct wsm_container *fs, struct wsm_workspace *ws,
	int width, int height);

#endif
