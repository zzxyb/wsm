#ifndef WSM_WINDOW_H
#define WSM_WINDOW_H

#include "node/wsm_node.h"

#include <wayland-server-core.h>

#include <wlr/util/box.h>

#define MIN_SANE_W 100
#define MIN_SANE_H 60

struct wlr_scene_buffer;

struct wsm_view;
struct wsm_titlebar;
struct wsm_view_item;

/**
 * @brief Enumeration of border styles for a wsm_window
 */
enum wsm_window_border {
	B_NONE, /**< No border */
	B_NORMAL, /**< Normal border */
	B_CSD, /**< Client-side decoration border */
};

/**
 * @brief Enumeration of fullscreen modes for a wsm_window
 */
enum wsm_fullscreen_mode {
	FULLSCREEN_NONE, /**< No fullscreen mode */
	FULLSCREEN_WORKSPACE, /**< Fullscreen mode for the workspace */
	FULLSCREEN_GLOBAL, /**< Global fullscreen mode */
};

/**
 * @brief Enumeration of alignment options
 */
enum alignment {
	ALIGN_LEFT, /**< Align to the left */
	ALIGN_CENTER, /**< Center alignment */
	ALIGN_RIGHT, /**< Align to the right */
};

/**
 * @brief Structure representing border colors for a wsm_window
 */
struct border_colors {
	float border[4]; /**< RGBA values for the border color */
	float background[4]; /**< RGBA values for the background color */
	float text[4]; /**< RGBA values for the text color */
	float indicator[4]; /**< RGBA values for the indicator color */
	float window_border[4]; /**< RGBA values for the window border color */
};

/**
 * @brief Structure representing the state of a wsm_window
 */
struct wsm_window_state {
	struct wsm_workspace *workspace; /**< Pointer to the associated workspace */

	double x, y; /**< Position of the window */
	double width, height; /**< Dimensions of the window */
	double content_x, content_y; /**< Position of the content within the window */
	double content_width, content_height; /**< Dimensions of the content within the window */

	enum wsm_fullscreen_mode fullscreen_mode; /**< Current fullscreen mode of the window */
	enum wsm_window_border border; /**< Current border style of the window */
	int border_thickness; /**< Thickness of the border */
	int sensing_thickness; /**< Thickness for sensing borders */
	bool border_top; /**< Flag indicating if the top border is present */
	bool border_bottom; /**< Flag indicating if the bottom border is present */
	bool border_left; /**< Flag indicating if the left border is present */
	bool border_right; /**< Flag indicating if the right border is present */
	bool focused; /**< Flag indicating if the window is focused */
};

/**
 * @brief Structure representing a managed window.
 */
struct wsm_window {
	struct wsm_window_state current; /**< Current state of the window */
	struct wsm_window_state pending; /**< Pending state of the window */

	struct wsm_node node; /**< Node representing the window in the scene graph */

	struct {
		struct wlr_scene_tree *tree; /**< Scene tree for the window */

		struct wlr_scene_rect *top; /**< Top sensing rectangle */
		struct wlr_scene_rect *bottom; /**< Bottom sensing rectangle */
		struct wlr_scene_rect *left; /**< Left sensing rectangle */
		struct wlr_scene_rect *right; /**< Right sensing rectangle */
	} sensing; /**< Sensing rectangles for the window */

	struct wl_listener output_enter; /**< Listener for output enter events */
	struct wl_listener output_leave; /**< Listener for output leave events */

	struct wlr_box transform; /**< Transformation box for the window */

	struct wsm_view *view; /**< Pointer to the associated wsm_view */

	struct wlr_scene_tree *scene_tree; /**< Scene tree for the window's content */

	struct wsm_titlebar *title_bar; /**< Pointer to the title bar of the window */

	struct wlr_scene_tree *content_tree; /**< Scene tree for the window's content */
	struct wlr_scene_buffer *output_handler; /**< Output handler for the window */

	char *title;           /**< The view's title (unformatted) */
	char *formatted_title; /**< The title displayed in the title bar */

	double saved_x, saved_y; /**< Saved position of the window */
	double saved_width, saved_height; /**< Saved dimensions of the window */
	double saved_maximized_x, saved_maximized_y; /**< Saved position before maximizing */
	double saved_maximized_width, saved_maximized_height; /**< Saved size before maximizing */
	double saved_maximized_content_x, saved_maximized_content_y; /**< Saved content position before maximizing */
	double saved_maximized_content_width, saved_maximized_content_height; /**< Saved content size before maximizing */
	float alpha; /**< Alpha transparency of the window */
	int title_width; /**< Width of the title bar */
	enum wsm_window_border saved_border; /**< Saved border style of the window */

	bool scratchpad; /**< Flag indicating if the window is a scratchpad */
	bool is_sticky; /**< Flag indicating if the window is sticky */
	bool maximized; /**< Flag indicating if the window is maximized */
};

/**
 * @brief Creates a new wsm_window instance
 * @param view Pointer to the associated wsm_view
 * @return Pointer to the newly created wsm_window instance
 */
struct wsm_window *window_create(struct wsm_view *view);

/**
 * @brief Destroys the specified wsm_window instance
 * @param window Pointer to the wsm_window instance to destroy
 */
void window_destroy(struct wsm_window *window);

/**
 * @brief Begins the destruction process for the specified wsm_window
 * @param window Pointer to the wsm_window instance to begin destruction for
 */
void window_begin_destroy(struct wsm_window *window);

/**
 * @brief Gets the bounding box for the specified wsm_window
 * @param window Pointer to the wsm_window instance
 * @param box Pointer to the wlr_box to store the bounding box
 */
void window_get_box(struct wsm_window *window, struct wlr_box *box);

/**
 * @brief Checks if the window is fullscreen
 * @param window Pointer to the wsm_window to check
 * @return true if the window is fullscreen, false otherwise
 */
bool window_is_fullscreen(struct wsm_window *window);

/**
 * @brief Checks if the scratchpad is hidden
 * @param window Pointer to the wsm_window to check
 * @return true if the scratchpad is hidden, false otherwise
 */
bool window_is_scratchpad_hidden(struct wsm_window *window);

/**
 * @brief Checks if the window belongs to a workspace or scratchpad
 * @param window Pointer to the wsm_window to check
 * @return true if the window is managed by a workspace or scratchpad, false otherwise
 */
bool window_is_managed(struct wsm_window *window);

/**
 * @brief Gets the height of the title bar
 * @return Height of the title bar
 */
size_t window_titlebar_height(void);

/**
 * @brief Raises the specified window
 * @param window Pointer to the wsm_window to raise
 */
bool window_raise(struct wsm_window *window);

/**
 * @brief Sets the maximized state for a window
 * @param window Pointer to the wsm_window to maximize or restore
 * @param maximized true to maximize, false to restore
 */
void window_set_maximized(struct wsm_window *window, bool maximized);

/**
 * @brief Minimizes a window's view
 * @param window Pointer to the wsm_window to minimize
 */
void window_minimize(struct wsm_window *window);

/**
 * @brief Disables fullscreen mode for the specified window
 * @param window Pointer to the wsm_window to disable fullscreen for
 */
void window_fullscreen_disable(struct wsm_window *window);

/**
 * @brief Moves a window to the specified coordinates
 * @param window Pointer to the wsm_window to move
 * @param lx X coordinate to move to
 * @param ly Y coordinate to move to
 */
void window_move_to(struct wsm_window *window,
	double lx, double ly);

/**
 * @brief Moves a window to the center of the screen
 * @param window Pointer to the wsm_window to center
 */
void window_move_to_center(struct wsm_window *window);

/**
 * @brief Translates a window by the specified amounts
 * @param window Pointer to the wsm_window to translate
 * @param x_amount Amount to translate in the X direction
 * @param y_amount Amount to translate in the Y direction
 */
void window_translate(struct wsm_window *window,
	double x_amount, double y_amount);

/**
 * @brief Resizes and centers a window
 * @param window Pointer to the wsm_window to resize and center
 */
void window_resize_and_center(struct wsm_window *window);

/**
 * @brief Sets the geometry of the window based on its content
 * @param window Pointer to the wsm_window to set geometry for
 */
void window_set_geometry_from_content(struct wsm_window *window);

/**
 * @brief Ends mouse operation for the specified window
 * @param window Pointer to the wsm_window to end mouse operation for
 */
void window_end_mouse_operation(struct wsm_window *window);

/**
 * @brief Detaches a window from its workspace
 * @param window Pointer to the wsm_window to detach
 */
void window_detach(struct wsm_window *window);

/**
 * @brief Builds a representation of a window list
 * @param windows List of windows
 * @param buffer Buffer to store the representation
 * @return Size of the built representation
 */
size_t window_build_representation(struct wsm_list *windows, char *buffer);

/**
 * @brief Updates the title bar of the specified window
 * @param window Pointer to the wsm_window to update the title bar for
 */
void window_update_title_bar(struct wsm_window *window);

/**
 * @brief Handles fullscreen reparenting for the specified window
 * @param window Pointer to the wsm_window to handle reparenting for
 */
void window_handle_fullscreen_reparent(struct wsm_window *window);

/**
 * @brief Fixes the coordinates of a window after output geometry changes
 * @param window Pointer to the wsm_window to fix coordinates for
 * @param old Pointer to the old wlr_box
 * @param new Pointer to the new wlr_box
 */
void window_fix_coordinates(struct wsm_window *window,
	struct wlr_box *old, struct wlr_box *new);

/**
 * @brief Sets the fullscreen mode for the specified window
 * @param window Pointer to the wsm_window to set fullscreen for
 * @param mode Fullscreen mode to set
 */
void window_set_fullscreen(struct wsm_window *window,
	enum wsm_fullscreen_mode mode);

/**
 * @brief Removes a window from the root scratchpad
 * @param window Pointer to the wsm_window to remove
 */
void root_scratchpad_remove_window(struct wsm_window *window);

/**
 * @brief Sets the default size for a window
 * @param window Pointer to the wsm_window to set the default size for
 */
void window_set_default_size(struct wsm_window *window);

/**
 * @brief Checks if the specified window is sticky
 * @param window Pointer to the wsm_window to check
 * @return true if the window is sticky, false otherwise
 */
bool window_is_sticky(struct wsm_window *window);

/**
 * @brief Checks if the specified window is transient for the given ancestor
 * @param child Pointer to the transient wsm_window to check
 * @param ancestor Pointer to the ancestor wsm_window
 * @return true if the window is transient for the ancestor, false otherwise
 */
bool window_is_transient_for(struct wsm_window *child,
	struct wsm_window *ancestor);

/**
 * @brief Updates the specified window
 * @param window Pointer to the wsm_window to update
 */
void window_update(struct wsm_window *window);

/**
 * @brief Updates the specified window
 * @param window Pointer to the wsm_window to update
 */
void window_update_itself_and_parents(struct wsm_window *window);

/**
 * @brief Sets the resizing state for the specified window
 * @param window Pointer to the wsm_window to set resizing for
 * @param resizing Flag indicating if the window is resizing
 */
void window_set_resizing(struct wsm_window *window, bool resizing);

/**
 * @brief Calculates configured window size constraints
 * @param min_width Pointer to store the minimum width
 * @param max_width Pointer to store the maximum width
 * @param min_height Pointer to store the minimum height
 * @param max_height Pointer to store the maximum height
 */
void window_calculate_constraints(int *min_width, int *max_width,
	int *min_height, int *max_height);

/**
 * @brief Retrieves the fullscreen window obstructing the specified window
 * @param window Pointer to the wsm_window to check
 * @return Pointer to the obstructing fullscreen window
 */
struct wsm_window *window_obstructing_fullscreen_window(struct wsm_window *window);

/**
 * @brief Disables the specified window
 * @param window Pointer to the wsm_window to disable
 */
void disable_window(struct wsm_window *window);

/**
 * @brief Gets the maximum thickness of the specified window state
 * @param state The state of the wsm_window
 * @return Maximum thickness value
 */
int get_max_thickness(struct wsm_window_state state);

#endif
