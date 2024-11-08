#ifndef WSM_KEYBOARD_H
#define WSM_KEYBOARD_H

#include <wayland-server-core.h>

#include <xkbcommon/xkbcommon.h>

#define WSM_KEYBOARD_PRESSED_KEYS_CAP 32

struct wlr_keyboard;
struct wlr_keyboard_group;
struct wlr_keyboard_shortcuts_inhibitor_v1;

struct xkb_keymap;

struct wsm_seat;
struct wsm_list;
struct wsm_seat_device;

/**
 * @brief Enumeration of binding input types
 */
enum binding_input_type {
	BINDING_KEYCODE, /**< Keycode binding type */
	BINDING_KEYSYM, /**< Keysym binding type */
	BINDING_MOUSECODE, /**< Mouse code binding type */
	BINDING_MOUSESYM, /**< Mouse symbol binding type */
	BINDING_SWITCH, /**< Dummy binding type, only used to call seat_execute_command */
	BINDING_GESTURE /**< Dummy binding type, only used to call seat_execute_command */
};

/**
 * @brief Enumeration of binding flags
 */
enum binding_flags {
	BINDING_RELEASE = 1 << 0, /**< Flag indicating release action */
	BINDING_LOCKED = 1 << 1, /**< Flag indicating the binding is locked (keyboard only) */
	BINDING_BORDER = 1 << 2, /**< Flag indicating trigger on container border (mouse only) */
	BINDING_CONTENTS = 1 << 3, /**< Flag indicating trigger on container contents (mouse only) */
	BINDING_TITLEBAR = 1 << 4, /**< Flag indicating trigger on container titlebar (mouse only) */
	BINDING_CODE = 1 << 5, /**< Flag indicating conversion of keysyms into keycodes (keyboard only) */
	BINDING_RELOAD = 1 << 6, /**< Flag indicating (re)trigger binding on reload (switch only) */
	BINDING_INHIBITED = 1 << 7, /**< Flag indicating to ignore shortcut inhibitor (keyboard only) */
	BINDING_NOREPEAT = 1 << 8, /**< Flag indicating not to trigger when repeating a held key (keyboard only) */
	BINDING_EXACT = 1 << 9, /**< Flag indicating to only trigger on exact match (gesture only) */
};

/**
 * @brief Structure representing a binding in the WSM
 */
struct wsm_binding {
	struct wsm_list *keys; /**< List of keys sorted in ascending order */
	struct wsm_list *syms; /**< List of syms sorted in ascending order; NULL if BINDING_CODE is not set */
	char *command; /**< Command associated with the binding */
	char *input; /**< Input associated with the binding */
	enum binding_input_type type; /**< Type of the binding input */
	int order; /**< Order of the binding */
	uint32_t flags; /**< Flags associated with the binding */
	uint32_t modifiers; /**< Modifiers for the binding */
	xkb_layout_index_t group; /**< Group for the binding */
};

/**
 * @brief Structure representing the state of keyboard shortcuts
 */
struct wsm_shortcut_state {
	uint32_t pressed_keys[WSM_KEYBOARD_PRESSED_KEYS_CAP]; /**< Array of currently pressed keys */
	uint32_t pressed_keycodes[WSM_KEYBOARD_PRESSED_KEYS_CAP]; /**< Array of currently pressed keycodes */
	size_t npressed; /**< Number of currently pressed keys */
	uint32_t last_keycode; /**< Last keycode pressed */
	uint32_t last_raw_modifiers; /**< Last raw modifiers pressed */
	uint32_t current_key; /**< Current key being processed */
};

/**
 * @brief Structure representing a keyboard in the WSM
 */
struct wsm_keyboard {
	struct wsm_shortcut_state state_keysyms_translated; /**< State for translated keysyms */
	struct wsm_shortcut_state state_keysyms_raw; /**< State for raw keysyms */
	struct wsm_shortcut_state state_keycodes; /**< State for keycodes */
	struct wsm_shortcut_state state_pressed_sent; /**< State for pressed keys sent */

	struct wl_listener keyboard_key; /**< Listener for keyboard key events */
	struct wl_listener keyboard_modifiers; /**< Listener for keyboard modifier events */

	struct wsm_binding *held_binding; /**< Pointer to the currently held binding */
	struct wl_event_source *key_repeat_source; /**< Pointer to the key repeat event source */
	struct wsm_binding *repeat_binding; /**< Pointer to the binding for repeat action */
	struct wsm_seat_device *device_wsm; /**< Pointer to the associated WSM seat device */
	struct wlr_keyboard *keyboard_wlr; /**< Pointer to the WLR keyboard instance */

	struct xkb_keymap *keymap; /**< Pointer to the XKB keymap */
	xkb_layout_index_t effective_layout; /**< Effective layout index */

	int32_t repeat_rate; /**< Rate of key repeat */
	int32_t repeat_delay; /**< Delay before key repeat starts */
};

/**
 * @brief Structure representing a group of keyboards in the WSM
 */
struct wsm_keyboard_group {
	struct wl_listener keyboard_key; /**< Listener for keyboard key events */
	struct wl_listener keyboard_modifiers; /**< Listener for keyboard modifier events */
	struct wl_listener enter; /**< Listener for enter events */
	struct wl_listener leave; /**< Listener for leave events */
	struct wl_list link; /**< Link for managing a list of keyboard groups */

	struct wlr_keyboard_group *keyboard_group_wlr; /**< Pointer to the WLR keyboard group */
	struct wsm_seat_device *seat_device; /**< Pointer to the associated WSM seat device */
};

/**
 * @brief Structure representing a keyboard shortcuts inhibitor
 */
struct wsm_keyboard_shortcuts_inhibitor {
	struct wl_listener destroy; /**< Listener for destruction events */

	struct wl_list link; /**< Link for managing a list of keyboard shortcuts inhibitors */

	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor_wlr; /**< Pointer to the WLR keyboard shortcuts inhibitor */
};

/**
 * @brief Creates a new wsm_keyboard instance
 * @param seat Pointer to the WSM seat associated with the keyboard
 * @param device Pointer to the WSM seat device associated with the keyboard
 * @return Pointer to the newly created wsm_keyboard instance
 */
struct wsm_keyboard *wsm_keyboard_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);

/**
 * @brief Configures the specified wsm_keyboard instance
 * @param keyboard Pointer to the wsm_keyboard instance to configure
 */
void wsm_keyboard_configure(struct wsm_keyboard *keyboard);

/**
 * @brief Destroys the specified wsm_keyboard instance
 * @param keyboard Pointer to the wsm_keyboard instance to destroy
 */
void wsm_keyboard_destroy(struct wsm_keyboard *keyboard);

/**
 * @brief Disarms key repeat for the specified wsm_keyboard instance
 * @param keyboard Pointer to the wsm_keyboard instance to disarm
 */
void wsm_keyboard_disarm_key_repeat(struct wsm_keyboard *keyboard);

/**
 * @brief Retrieves the wsm_keyboard associated with the specified WLR keyboard
 * @param seat Pointer to the WSM seat associated with the keyboard
 * @param wlr_keyboard Pointer to the WLR keyboard instance
 * @return Pointer to the associated wsm_keyboard instance
 */
struct wsm_keyboard *wsm_keyboard_for_wlr_keyboard(
	struct wsm_seat *seat, struct wlr_keyboard *wlr_keyboard);

/**
 * @brief Retrieves the modifier mask by name
 * @param name Pointer to the name of the modifier
 * @return Modifier mask corresponding to the name
 */
uint32_t get_modifier_mask_by_name(const char *name);

/**
 * @brief Retrieves the name of a modifier by its mask
 * @param modifier Modifier mask
 * @return Pointer to the name string of the modifier
 */
const char *get_modifier_name_by_mask(uint32_t modifier);

/**
 * @brief Retrieves the names of modifiers based on their masks
 * @param names Array to be populated with modifier names
 * @param modifier_masks Modifier masks to check
 * @return Number of modifier names retrieved
 */
int get_modifier_names(const char **names, uint32_t modifier_masks);

/**
 * @brief Retrieves the keyboard shortcuts inhibitor for the focused surface
 * @param seat Pointer to the WSM seat
 * @return Pointer to the associated wsm_keyboard_shortcuts_inhibitor
 */
struct wsm_keyboard_shortcuts_inhibitor *
keyboard_shortcuts_inhibitor_get_for_focused_surface(
	const struct wsm_seat *seat);

#endif
