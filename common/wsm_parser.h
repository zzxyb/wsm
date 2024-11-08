#ifndef WSM_PARSER_H
#define WSM_PARSER_H

#include <stdbool.h>
#include <limits.h>

#include <wayland-util.h>

/**
 * @brief Structure representing a configuration entry.
 */
struct wsm_config_entry {
	struct wl_list link; /**< Link to the next entry in the list */
	char *key; /**< Key of the configuration entry */
	char *value; /**< Value of the configuration entry */
};

/**
 * @brief Structure representing a section of configuration entries.
 */
struct wsm_config_section {
	struct wl_list entry_list; /**< List of configuration entries in this section */
	struct wl_list link; /**< Link to the next section in the list */
	char *name; /**< Name of the configuration section */
};

/**
 * @brief Enumeration of option types for configuration.
 */
enum wsm_option_type {
	WSM_OPTION_INTEGER, /**< Integer option type */
	WSM_OPTION_UNSIGNED_INTEGER, /**< Unsigned integer option type */
	WSM_OPTION_STRING, /**< String option type */
	WSM_OPTION_BOOLEAN /**< Boolean option type */
};

/**
 * @brief Structure representing a configuration option.
 */
struct wsm_option {
	enum wsm_option_type type; /**< Type of the option */
	const char *name; /**< Name of the option */
	char short_name; /**< Short name for the option */
	void *data; /**< Pointer to the data associated with the option */
};

/**
 * @brief Structure representing the overall configuration.
 */
struct wsm_config {
	struct wl_list section_list; /**< List of configuration sections */
	char path[PATH_MAX]; /**< Path to the configuration file */
};

/**
 * @brief Parses command line options.
 * @param options Pointer to an array of wsm_option structures.
 * @param count Number of options in the array.
 * @param argc Pointer to the argument count.
 * @param argv Pointer to the argument vector.
 * @return 0 on success, or a negative error code on failure.
 */
int parse_options(const struct wsm_option *options, int count, int *argc, char *argv[]);

/**
 * @brief Retrieves an integer value from a configuration section.
 * @param section Pointer to the wsm_config_section instance.
 * @param key Key of the configuration entry.
 * @param value Pointer to store the retrieved integer value.
 * @param default_value Default value to return if the key is not found.
 * @return 0 on success, or a negative error code on failure.
 */
int wsm_config_section_get_int(struct wsm_config_section *section, const char *key,
	int32_t *value, int32_t default_value);

/**
 * @brief Retrieves an unsigned integer value from a configuration section.
 * @param section Pointer to the wsm_config_section instance.
 * @param key Key of the configuration entry.
 * @param value Pointer to store the retrieved unsigned integer value.
 * @param default_value Default value to return if the key is not found.
 * @return 0 on success, or a negative error code on failure.
 */
int wsm_config_section_get_uint(struct wsm_config_section *section, const char *key,
	uint32_t *value, uint32_t default_value);

/**
 * @brief Retrieves a double value from a configuration section.
 * @param section Pointer to the wsm_config_section instance.
 * @param key Key of the configuration entry.
 * @param value Pointer to store the retrieved double value.
 * @param default_value Default value to return if the key is not found.
 * @return 0 on success, or a negative error code on failure.
 */
int wsm_config_section_get_double(struct wsm_config_section *section, const char *key,
	double *value, double default_value);

/**
 * @brief Retrieves a string value from a configuration section.
 * @param section Pointer to the wsm_config_section instance.
 * @param key Key of the configuration entry.
 * @param value Pointer to store the retrieved string value.
 * @param default_value Default value to return if the key is not found.
 * @return 0 on success, or a negative error code on failure.
 */
int wsm_config_section_get_string(struct wsm_config_section *section, const char *key, char **value,
	const char *default_value);

/**
 * @brief Retrieves a boolean value from a configuration section.
 * @param section Pointer to the wsm_config_section instance.
 * @param key Key of the configuration entry.
 * @param value Pointer to store the retrieved boolean value.
 * @param default_value Default value to return if the key is not found.
 * @return 0 on success, or a negative error code on failure.
 */
int wsm_config_section_get_bool(struct wsm_config_section *section, const char *key,
	bool *value, bool default_value);

#endif
