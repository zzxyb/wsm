#ifndef WSM_WORKSPACE_H
#define WSM_WORKSPACE_H

#include "wsm_container.h"
#include "node/wsm_node.h"

#include <stdbool.h>

struct wlr_box;
struct wlr_surface;
struct wlr_scene_tree;

struct wsm_list;
struct wsm_output;
struct wsm_xwayland_view;

 /* -------------------------------------------------
 * |                                                |
 * |                wsm_workspace                   |
 * |                                                |
 * |    ***********                                 |
 * |    *         *  wsm_container1 mean layer1     |
 * |    *         *                                 |
 * |    *   ***********                             |
 * |    *   *     *   *                             |
 * |    *   *     *   * wsm_container2 mean layer2  |
 * |    ***********   *                             |
 * |        *         *                             |
 * |        *         *                             |
 * |        ***********                             |
 * |                         container3....max      |
 * --------------------------------------------------*/

/**
 * @brief Structure representing the state of a workspace.
 */
struct wsm_workspace_state {
	struct wsm_list *floating; /**< List of floating containers in the workspace */
	struct wsm_list *tiling; /**< List of tiled containers in the workspace */
	struct wsm_container *fullscreen; /**< Pointer to the fullscreen container */
	struct wsm_container *focused_inactive_child; /**< Pointer to the focused inactive child container */
	struct wsm_output *output; /**< Pointer to the associated output */

	double x, y; /**< Position of the workspace */
	int width, height; /**< Dimensions of the workspace */
	enum wsm_container_layout layout; /**< Current layout of the workspace */
	bool focused; /**< Flag indicating if the workspace is focused */
};

/**
 * @brief Structure representing the side gaps of a workspace.
 */
struct side_gaps {
	int top; /**< Top gap size */
	int right; /**< Right gap size */
	int bottom; /**< Bottom gap size */
	int left; /**< Left gap size */
};

/**
 * @brief Structure representing a workspace object.
 *
 * @details Multiple wsm_containers are arranged in layers
 * in wsm_workspace to achieve a stacking effect.
 */
struct wsm_workspace {
	struct wsm_workspace_state current; /**< Current state of the workspace */
	struct wsm_node node; /**< Node representing the workspace in the scene graph */

	struct {
		struct wlr_scene_tree *non_fullscreen; /**< Scene tree for non-fullscreen containers */
		struct wlr_scene_tree *fullscreen; /**< Scene tree for fullscreen containers */
	} layers; /**< Layers of the workspace */

	struct side_gaps current_gaps; /**< Current gaps in the workspace */
	struct side_gaps gaps_outer; /**< Outer gaps of the workspace */

	struct wsm_container *fullscreen; /**< Pointer to the fullscreen container */
	struct wsm_output *output; /**< Pointer to the associated output (NULL if no outputs are connected) */
	struct wsm_list *floating; /**< List of floating containers */
	struct wsm_list *tiling; /**< List of tiled containers */
	struct wsm_list *output_priority; /**< List of output priorities */

	char *name; /**< Name of the workspace */
	char *representation; /**< Representation string for the workspace */

	double x, y; /**< Position of the workspace */
	int width, height; /**< Dimensions of the workspace */
	int gaps_inner; /**< Inner gaps of the workspace */
	enum wsm_container_layout layout; /**< Current layout of the workspace */
	enum wsm_container_layout prev_split_layout; /**< Previous split layout of the workspace */

	bool urgent; /**< Flag indicating if the workspace is urgent */
};

/**
 * @brief Creates a new workspace.
 * @param output Pointer to the associated output.
 * @param name Name of the workspace.
 * @return Pointer to the newly created wsm_workspace instance.
 */
struct wsm_workspace *workspace_create(struct wsm_output *output,
	const char *name);

/**
 * @brief Destroys the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance to destroy.
 */
void workspace_destroy(struct wsm_workspace *workspace);

/**
 * @brief Detaches the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance to detach.
 */
void workspace_detach(struct wsm_workspace *workspace);

/**
 * @brief Gets the bounding box of the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 * @param box Pointer to the wlr_box to store the bounding box.
 */
void workspace_get_box(struct wsm_workspace *workspace, struct wlr_box *box);

/**
 * @brief Applies a function to each container in the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 * @param f Function to apply to each container.
 * @param data Additional data to pass to the function.
 */
void workspace_for_each_container(struct wsm_workspace *ws,
	void (*f)(struct wsm_container *con, void *data), void *data);

/**
 * @brief Checks if the specified workspace is visible.
 * @param ws Pointer to the wsm_workspace instance.
 * @return true if the workspace is visible, false otherwise.
 */
bool workspace_is_visible(struct wsm_workspace *ws);

/**
 * @brief Checks if the specified workspace is empty.
 * @param ws Pointer to the wsm_workspace instance.
 * @return true if the workspace is empty, false otherwise.
 */
bool workspace_is_empty(struct wsm_workspace *ws);

/**
 * @brief Applies a function to each container in the root workspace.
 * @param f Function to apply to each container.
 * @param data Additional data to pass to the function.
 */
void root_for_each_container(void (*f)(struct wsm_container *con, void *data), void *data);

/**
 * @brief Applies a function to each workspace in the root.
 * @param f Function to apply to each workspace.
 * @param data Additional data to pass to the function.
 */
void root_for_each_workspace(void (*f)(struct wsm_workspace *ws, void *data), void *data);

/**
 * @brief Updates the representation of the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 */
void workspace_update_representation(struct wsm_workspace *ws);

/**
 * @brief Adds a floating container to the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 * @param con Pointer to the wsm_container to add.
 */
void workspace_add_floating(struct wsm_workspace *workspace,
	struct wsm_container *con);

/**
 * @brief Adds gaps to the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 */
void workspace_add_gaps(struct wsm_workspace *ws);

/**
 * @brief Considers destroying the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 */
void workspace_consider_destroy(struct wsm_workspace *ws);

/**
 * @brief Begins the destruction process for the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 */
void workspace_begin_destroy(struct wsm_workspace *workspace);

/**
 * @brief Adds a tiling container to the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 * @param con Pointer to the wsm_container to add.
 * @return Pointer to the newly added wsm_container.
 */
struct wsm_container *workspace_add_tiling(struct wsm_workspace *workspace,
	struct wsm_container *con);

/**
 * @brief Detects urgent state for the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 */
void workspace_detect_urgent(struct wsm_workspace *workspace);

/**
 * @brief Finds a container in the specified workspace based on a test function.
 * @param ws Pointer to the wsm_workspace instance.
 * @param test Function to test each container.
 * @param data Additional data to pass to the test function.
 * @return Pointer to the found wsm_container, or NULL if not found.
 */
struct wsm_container *workspace_find_container(struct wsm_workspace *ws,
	bool (*test)(struct wsm_container *con, void *data), void *data);

/**
 * @brief Gets the highest available output for the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 * @param exclude Pointer to an output to exclude from consideration.
 * @return Pointer to the highest available wsm_output.
 */
struct wsm_output *workspace_output_get_highest_available(
	struct wsm_workspace *ws, struct wsm_output *exclude);

/**
 * @brief Counts the number of sticky containers in the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 * @return Number of sticky containers.
 */
size_t workspace_num_sticky_containers(struct wsm_workspace *ws);

/**
 * @brief Adds an output to the priority list of the specified workspace.
 * @param workspace Pointer to the wsm_workspace instance.
 * @param output Pointer to the wsm_output to add.
 */
void workspace_output_add_priority(struct wsm_workspace *workspace,
	struct wsm_output *output);

/**
 * @brief Checks if the specified output matches a name or ID.
 * @param output Pointer to the wsm_output instance.
 * @param name_or_id Name or ID to match against.
 * @return true if there is a match, false otherwise.
 */
bool output_match_name_or_id(struct wsm_output *output,
	const char *name_or_id);

/**
 * @brief Adds a workspace to the specified output.
 * @param output Pointer to the wsm_output instance.
 * @param workspace Pointer to the wsm_workspace to add.
 */
void output_add_workspace(struct wsm_output *output,
	struct wsm_workspace *workspace);

/**
 * @brief Sorts the workspaces associated with the specified output.
 * @param output Pointer to the wsm_output instance.
 */
void output_sort_workspaces(struct wsm_output *output);

/**
 * @brief Disables the specified workspace.
 * @param ws Pointer to the wsm_workspace instance.
 */
void disable_workspace(struct wsm_workspace *ws);

#endif
