#ifndef WSM_CONTAINER_H
#define WSM_CONTAINER_H

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
 * @brief Enumeration of layout statuses for wsm_view_items in a wsm_container
 *
 * @details If the current wsm_container has only one wsm_view_item,
 * then the wsm_container is equivalent to one wsm_view_item. Note that
 * there are still layer levels between wsm_view_items at this time.
 */
enum wsm_container_layout {
	L_NONE, /**< No layout; the wsm_container is equivalent to a single wsm_view_item. */
	L_HORIZ, /**< Two wsm_view_items in a horizontal layout. */
	L_HORIZ_1_V_2, /**< Three wsm_view_items in a horizontal layout; left : right = 1 : 2. */
	L_HORIZ_2_V_1, /**< Three wsm_view_items in a horizontal layout; left : right = 2 : 1. */
	L_GRID, /**< Four wsm_view_items in a grid layout. */
};

/**
 * @brief Enumeration of border styles for a wsm_container
 */
enum wsm_container_border {
	B_NONE, /**< No border */
	B_NORMAL, /**< Normal border */
	B_CSD, /**< Client-side decoration border */
};

/**
 * @brief Enumeration of fullscreen modes for a wsm_container
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
 * @brief Structure representing border colors for a wsm_container
 */
struct border_colors {
	float border[4]; /**< RGBA values for the border color */
	float background[4]; /**< RGBA values for the background color */
	float text[4]; /**< RGBA values for the text color */
	float indicator[4]; /**< RGBA values for the indicator color */
	float child_border[4]; /**< RGBA values for the child border color */
};

/**
 * @brief Structure representing the state of a wsm_container
 */
struct wsm_container_state {
	struct wsm_list *children; /**< List of child containers */
	struct wsm_workspace *workspace; /**< Pointer to the associated workspace */
	struct wsm_container *parent; /**< Pointer to the parent container */

	struct wsm_container *focused_inactive_child; /**< Pointer to the focused inactive child container */

	double x, y; /**< Position of the container */
	double width, height; /**< Dimensions of the container */
	double content_x, content_y; /**< Position of the content within the container */
	double content_width, content_height; /**< Dimensions of the content within the container */

	enum wsm_container_layout layout; /**< Current layout of the container */
	enum wsm_fullscreen_mode fullscreen_mode; /**< Current fullscreen mode of the container */
	enum wsm_container_border border; /**< Current border style of the container */
	int border_thickness; /**< Thickness of the border */
	int sensing_thickness; /**< Thickness for sensing borders */
	bool border_top; /**< Flag indicating if the top border is present */
	bool border_bottom; /**< Flag indicating if the bottom border is present */
	bool border_left; /**< Flag indicating if the left border is present */
	bool border_right; /**< Flag indicating if the right border is present */
	bool focused; /**< Flag indicating if the container is focused */
};

 /* L_NONE looks like this:
 * ------------------------
 * |                      |
 * | wsm_container L_NONE |
 * |                      |
 * | ******************   |
 * | *                *   |
 * | *  wsm_view_item *   |
 * | *                *   |
 * | ******************   |
 * ------------------------
 *
 * L_HORIZ looks like this:
 * --------------------------------------------
 * |           wsm_container L_HORIZ          |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item1    * |  *   view_item2   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_HORIZ_1_V_2 looks like this:
 * --------------------------------------------
 * |       wsm_container L_HORIZ_1_V_2        |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   view_item2   * |
 * | *                * |  *                * |
 * | *                * |  ****************** |
 * | *                * |                     |
 * | *   view_item1   * |  ---------split-----|
 * | *                * |                     |
 * | *                * |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   view_item3   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_HORIZ_2_V_1 looks like this:
 * --------------------------------------------
 * |       wsm_container L_HORIZ_2_V_1        |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item2    * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  *                * |
 * |                    |  *                * |
 * | ------split--------|  *   view_item1   * |
 * |                    |  *                * |
 * | ****************** |  *                * |
 * | *                * |  *                * |
 * | *  view_item3    * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_GRID looks like this:
 * --------------------------------------------
 * |           wsm_container L_GRID           |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item1    * |  *   view_item3   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * |                                          |
 * | ------split--------   --------split----- |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item2    * |  *   view_item4   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------*/

/**
 * @brief Structure representing a container for wsm_view_items
 *
 * @details This structure implements the layout functionality for
 * wsm_view_items.
 */
struct wsm_container {
	struct wsm_container_state current; /**< Current state of the container */
	struct wsm_container_state pending; /**< Pending state of the container */

	struct wsm_node node; /**< Node representing the container in the scene graph */

	struct {
		struct wlr_scene_tree *tree; /**< Scene tree for the container */

		struct wlr_scene_rect *top; /**< Top sensing rectangle */
		struct wlr_scene_rect *bottom; /**< Bottom sensing rectangle */
		struct wlr_scene_rect *left; /**< Left sensing rectangle */
		struct wlr_scene_rect *right; /**< Right sensing rectangle */
	} sensing; /**< Sensing rectangles for the container */

	struct wl_listener output_enter; /**< Listener for output enter events */
	struct wl_listener output_leave; /**< Listener for output leave events */

	struct wlr_box transform; /**< Transformation box for the container */

	struct wsm_view *view; /**< Pointer to the associated wsm_view */

	struct wlr_scene_tree *scene_tree; /**< Scene tree for the container's content */

	struct wsm_titlebar *title_bar; /**< Pointer to the title bar of the container */

	struct wlr_scene_tree *content_tree; /**< Scene tree for the container's content */
	struct wlr_scene_buffer *output_handler; /**< Output handler for the container */

	char *title;           /**< The view's title (unformatted) */
	char *formatted_title; /**< The title displayed in the title bar */

	double saved_x, saved_y; /**< Saved position of the container */
	double saved_width, saved_height; /**< Saved dimensions of the container */
	double width_fraction; /**< Fraction of the width */
	double height_fraction; /**< Fraction of the height */
	double child_total_width; /**< Total width of child containers */
	double child_total_height; /**< Total height of child containers */

	float alpha; /**< Alpha transparency of the container */
	int title_width; /**< Width of the title bar */
	enum wsm_container_layout prev_split_layout; /**< Previous split layout of the container */
	enum wsm_container_border saved_border; /**< Saved border style of the container */

	bool scratchpad; /**< Flag indicating if the container is a scratchpad */
	bool is_sticky; /**< Flag indicating if the container is sticky */
};

/**
 * @brief Creates a new wsm_container instance
 * @param view Pointer to the associated wsm_view
 * @return Pointer to the newly created wsm_container instance
 */
struct wsm_container *container_create(struct wsm_view *view);

/**
 * @brief Destroys the specified wsm_container instance
 * @param con Pointer to the wsm_container instance to destroy
 */
void container_destroy(struct wsm_container *con);

/**
 * @brief Begins the destruction process for the specified wsm_container
 * @param con Pointer to the wsm_container instance to begin destruction for
 */
void container_begin_destroy(struct wsm_container *con);

/**
 * @brief Gets the bounding box for the specified wsm_container
 * @param container Pointer to the wsm_container instance
 * @param box Pointer to the wlr_box to store the bounding box
 */
void container_get_box(struct wsm_container *container, struct wlr_box *box);

/**
 * @brief Finds a child container that matches a given test function
 * @param container Pointer to the parent wsm_container
 * @param test Function pointer to the test function
 * @param data Pointer to additional data for the test function
 * @return Pointer to the matching child container, or NULL if not found
 */
struct wsm_container *container_find_child(struct wsm_container *container,
	bool (*test)(struct wsm_container *view, void *data), void *data);

/**
 * @brief Executes a function for each child container
 * @param container Pointer to the parent wsm_container
 * @param f Function pointer to the function to execute
 * @param data Pointer to additional data for the function
 */
void container_for_each_child(struct wsm_container *container,
	void (*f)(struct wsm_container *container, void *data), void *data);

/**
 * @brief Checks if the container is fullscreen or has a fullscreen child
 * @param container Pointer to the wsm_container to check
 * @return true if the container is fullscreen or has a fullscreen child, false otherwise
 */
bool container_is_fullscreen_or_child(struct wsm_container *container);

/**
 * @brief Checks if the scratchpad is hidden
 * @param con Pointer to the wsm_container to check
 * @return true if the scratchpad is hidden, false otherwise
 */
bool container_is_scratchpad_hidden(struct wsm_container *con);

/**
 * @brief Checks if the scratchpad is hidden or has a hidden child
 * @param con Pointer to the wsm_container to check
 * @return true if the scratchpad is hidden or has a hidden child, false otherwise
 */
bool container_is_scratchpad_hidden_or_child(struct wsm_container *con);

/**
 * @brief Retrieves the top-level ancestor of the specified container
 * @param container Pointer to the wsm_container to find the ancestor for
 * @return Pointer to the top-level ancestor container
 */
struct wsm_container *container_toplevel_ancestor(
	struct wsm_container *container);

/**
 * @brief Checks if the container is floating or has a floating child
 * @param container Pointer to the wsm_container to check
 * @return true if the container is floating or has a floating child, false otherwise
 */
bool container_is_floating_or_child(struct wsm_container *container);

/**
 * @brief Checks if the container is floating
 * @param container Pointer to the wsm_container to check
 * @return true if the container is floating, false otherwise
 */
bool container_is_floating(struct wsm_container *container);

/**
 * @brief Gets the height of the title bar
 * @return Height of the title bar
 */
size_t container_titlebar_height(void);

/**
 * @brief Raises the specified floating container
 * @param con Pointer to the wsm_container to raise
 */
void container_raise_floating(struct wsm_container *con);

/**
 * @brief Disables fullscreen mode for the specified container
 * @param con Pointer to the wsm_container to disable fullscreen for
 */
void container_fullscreen_disable(struct wsm_container *con);

/**
 * @brief Moves a floating container to the specified coordinates
 * @param con Pointer to the wsm_container to move
 * @param lx X coordinate to move to
 * @param ly Y coordinate to move to
 */
void container_floating_move_to(struct wsm_container *con,
	double lx, double ly);

/**
 * @brief Moves a floating container to the center of the screen
 * @param con Pointer to the wsm_container to center
 */
void container_floating_move_to_center(struct wsm_container *con);

/**
 * @brief Translates a floating container by the specified amounts
 * @param con Pointer to the wsm_container to translate
 * @param x_amount Amount to translate in the X direction
 * @param y_amount Amount to translate in the Y direction
 */
void container_floating_translate(struct wsm_container *con,
	double x_amount, double y_amount);

/**
 * @brief Resizes and centers a floating container
 * @param con Pointer to the wsm_container to resize and center
 */
void container_floating_resize_and_center(struct wsm_container *con);

/**
 * @brief Sets the geometry of the container based on its content
 * @param con Pointer to the wsm_container to set geometry for
 */
void container_set_geometry_from_content(struct wsm_container *con);

/**
 * @brief Ends mouse operation for the specified container
 * @param container Pointer to the wsm_container to end mouse operation for
 */
void container_end_mouse_operation(struct wsm_container *container);

/**
 * @brief Checks if the specified descendant has the given ancestor
 * @param descendant Pointer to the descendant container
 * @param ancestor Pointer to the ancestor container
 * @return true if the descendant has the ancestor, false otherwise
 */
bool container_has_ancestor(struct wsm_container *descendant,
	struct wsm_container *ancestor);

/**
 * @brief Detaches a child container from its parent
 * @param child Pointer to the child wsm_container to detach
 */
void container_detach(struct wsm_container *child);

/**
 * @brief Retrieves the siblings of the specified container
 * @param container Pointer to the wsm_container to get siblings for
 * @return List of sibling containers
 */
struct wsm_list *container_get_siblings(struct wsm_container *container);

/**
 * @brief Updates the representation of the specified container
 * @param container Pointer to the wsm_container to update
 */
void container_update_representation(struct wsm_container *container);

/**
 * @brief Builds a representation of the container based on its layout and children
 * @param layout Layout type of the container
 * @param children List of child containers
 * @param buffer Buffer to store the representation
 * @return Size of the built representation
 */
size_t container_build_representation(enum wsm_container_layout layout,
	struct wsm_list *children, char *buffer);

/**
 * @brief Updates the title bar of the specified container
 * @param container Pointer to the wsm_container to update the title bar for
 */
void container_update_title_bar(struct wsm_container *container);

/**
 * @brief Handles fullscreen reparenting for the specified container
 * @param con Pointer to the wsm_container to handle reparenting for
 */
void container_handle_fullscreen_reparent(struct wsm_container *con);

/**
 * @brief Fixes the coordinates of a floating container
 * @param con Pointer to the wsm_container to fix coordinates for
 * @param old Pointer to the old wlr_box
 * @param new Pointer to the new wlr_box
 */
void floating_fix_coordinates(struct wsm_container *con,
	struct wlr_box *old, struct wlr_box *new);

/**
 * @brief Gets the parent layout of the specified container
 * @param con Pointer to the wsm_container to get the parent layout for
 * @return Parent layout of the container
 */
enum wsm_container_layout container_parent_layout(struct wsm_container *con);

/**
 * @brief Sets the fullscreen mode for the specified container
 * @param con Pointer to the wsm_container to set fullscreen for
 * @param mode Fullscreen mode to set
 */
void container_set_fullscreen(struct wsm_container *con,
	enum wsm_fullscreen_mode mode);

/**
 * @brief Reaps (removes) an empty container
 * @param con Pointer to the wsm_container to reap
 */
void container_reap_empty(struct wsm_container *con);

/**
 * @brief Removes a container from the root scratchpad
 * @param con Pointer to the wsm_container to remove
 */
void root_scratchpad_remove_container(struct wsm_container *con);

/**
 * @brief Adds a child container to the specified parent
 * @param parent Pointer to the parent wsm_container
 * @param child Pointer to the child wsm_container to add
 * @param after Flag indicating if the child should be added after the parent
 */
void container_add_sibling(struct wsm_container *parent,
	struct wsm_container *child, bool after);

/**
 * @brief Sets the floating state for the specified container
 * @param container Pointer to the wsm_container to set floating for
 * @param enable Flag indicating if the container should be floating
 */
void container_set_floating(struct wsm_container *container, bool enable);

/**
 * @brief Sets the default size for a floating container
 * @param con Pointer to the wsm_container to set the default size for
 */
void container_floating_set_default_size(struct wsm_container *con);

/**
 * @brief Adds a child container to the specified parent container
 * @param parent Pointer to the parent wsm_container
 * @param child Pointer to the child wsm_container to add
 */
void container_add_child(struct wsm_container *parent,
	struct wsm_container *child);

/**
 * @brief Checks if the specified container is sticky
 * @param con Pointer to the wsm_container to check
 * @return true if the container is sticky, false otherwise
 */
bool container_is_sticky(struct wsm_container *con);

/**
 * @brief Checks if the specified container is sticky or has a sticky child
 * @param con Pointer to the wsm_container to check
 * @return true if the container is sticky or has a sticky child, false otherwise
 */
bool container_is_sticky_or_child(struct wsm_container *con);

/**
 * @brief Checks if the specified child container is transient for the given ancestor
 * @param child Pointer to the child wsm_container to check
 * @param ancestor Pointer to the ancestor wsm_container
 * @return true if the child is transient for the ancestor, false otherwise
 */
bool container_is_transient_for(struct wsm_container *child,
	struct wsm_container *ancestor);

/**
 * @brief Updates the specified container
 * @param con Pointer to the wsm_container to update
 */
void container_update(struct wsm_container *con);

/**
 * @brief Updates the specified container and its parents
 * @param con Pointer to the wsm_container to update
 */
void container_update_itself_and_parents(struct wsm_container *con);

/**
 * @brief Checks if the specified container has any urgent child
 * @param container Pointer to the wsm_container to check
 * @return true if the container has an urgent child, false otherwise
 */
bool container_has_urgent_child(struct wsm_container *container);

/**
 * @brief Sets the resizing state for the specified container
 * @param con Pointer to the wsm_container to set resizing for
 * @param resizing Flag indicating if the container is resizing
 */
void container_set_resizing(struct wsm_container *con, bool resizing);

/**
 * @brief Calculates constraints for floating containers
 * @param min_width Pointer to store the minimum width
 * @param max_width Pointer to store the maximum width
 * @param min_height Pointer to store the minimum height
 * @param max_height Pointer to store the maximum height
 */
void floating_calculate_constraints(int *min_width, int *max_width,
	int *min_height, int *max_height);

/**
 * @brief Retrieves the container obstructing fullscreen for the specified container
 * @param container Pointer to the wsm_container to check
 * @return Pointer to the obstructing fullscreen container
 */
struct wsm_container *container_obstructing_fullscreen_container(struct wsm_container *container);

/**
 * @brief Disables the specified container
 * @param con Pointer to the wsm_container to disable
 */
void disable_container(struct wsm_container *con);

/**
 * @brief Gets the maximum thickness of the specified container state
 * @param state The state of the wsm_container
 * @return Maximum thickness value
 */
int get_max_thickness(struct wsm_container_state state);

#endif
