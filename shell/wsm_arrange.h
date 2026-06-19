#ifndef WSM_ARRANGE_H
#define WSM_ARRANGE_H

#include "wsm_window.h"

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
 * @brief Arranges the workspace in a windows layout
 * @param ws Pointer to the wsm_workspace to be arranged
 */
void arrange_workspace_windows(struct wsm_workspace *ws);

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
 * @brief Automatically arranges the specified window
 * @param window Pointer to the wsm_window to be arranged
 */
void wsm_arrange_window_auto(struct wsm_window *window);

/**
 * @brief Arranges the specified window with a title bar
 * @param window Pointer to the wsm_window to be arranged
 * @param width Desired width for the window
 * @param height Desired height for the window
 * @param title_bar Boolean indicating if a title bar should be included
 * @param gaps Gaps to be applied around the window
 */
void wsm_arrange_window_with_title_bar(struct wsm_window *window,
	int width, int height, bool title_bar, int gaps);

/**
 * @brief Arranges the windows of a window
 * @param windows Pointer to the list of windows to be arranged
 * @param parent Pointer to the parent box for the arrangement
 */
void wsm_arrange_window_list(struct wsm_list *windows, struct wlr_box *parent);

/**
 * @brief Arranges windows with a title bar
 * @param windows Pointer to the list of windows to be arranged
 * @param active Pointer to the active window
 * @param content Pointer to the scene tree containing the content
 * @param width Desired width for the arrangement
 * @param height Desired height for the arrangement
 * @param gaps Gaps to be applied around the arrangement
 */
void arrange_windows_with_titlebar(struct wsm_list *windows,
	struct wsm_window *active, struct wlr_scene_tree *content,
	int width, int height, int gaps);

/**
 * @brief Arranges the specified windows
 * @param windows Pointer to the list of windows to be arranged
 */
void wsm_arrange_windows(struct wsm_list *windows);

/**
 * @brief Arranges the title bar node of the specified window
 * @param window Pointer to the wsm_window whose title bar node will be arranged
 */
void window_arrange_title_bar_node(struct wsm_window *window);

/**
 * @brief Arranges the title bar of the specified window
 * @param window Pointer to the wsm_window whose title bar will be arranged
 * @param x X coordinate for the title bar
 * @param y Y coordinate for the title bar
 * @param width Desired width for the title bar
 * @param height Desired height for the title bar
 */
void wsm_arrange_title_bar(struct wsm_window *window,
	int x, int y, int width, int height);

/**
 * @brief Arranges the specified window in fullscreen mode
 * @param tree Pointer to the scene tree containing the fullscreen window
 * @param fs Pointer to the wsm_window to be arranged in fullscreen
 * @param ws Pointer to the wsm_workspace where the fullscreen window resides
 * @param width Desired width for the fullscreen arrangement
 * @param height Desired height for the fullscreen arrangement
 */
void wsm_arrange_fullscreen(struct wlr_scene_tree *tree,
	struct wsm_window *fs, struct wsm_workspace *ws,
	int width, int height);

#endif
