/**
 * @file        wsm_toml.h
 * @brief       Flat TOML document utility for wlframe.
 * @details     This file provides a small key/value TOML helper for loading,
 *              editing, querying, and saving scalar TOML values. Table entries
 *              are exposed as dotted keys, for example "output.name".
 * @author      YaoBing Xiao
 * @date        2026-07-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-07-21, initial version\n
 */

#ifndef UTIL_WSM_TOML_H
#define UTIL_WSM_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wayland-util.h>

/**
 * @brief Supported scalar TOML value types.
 */
enum wsm_toml_type {
	WSM_TOML_STRING,  /**< String value */
	WSM_TOML_INTEGER, /**< Signed integer value */
	WSM_TOML_DOUBLE,  /**< Floating point value */
	WSM_TOML_BOOLEAN, /**< Boolean value */
};

/**
 * @brief A single flattened TOML key/value entry.
 */
struct wsm_toml_entry {
	struct wl_list link;      /**< List node for wsm_toml::entries */
	char *key;                /**< Dotted TOML key */
	enum wsm_toml_type type;  /**< Stored value type */
	union {
		char *string;    /**< String value */
		int64_t integer; /**< Signed integer value */
		double decimal;  /**< Floating point value */
		bool boolean;    /**< Boolean value */
	} value;                 /**< Stored scalar value */
};

/**
 * @brief A flattened TOML document.
 */
struct wsm_toml {
	struct wl_list entries; /**< List of wsm_toml_entry::link */
};

/**
 * @brief Creates an empty TOML document.
 * @return New TOML document, or NULL on allocation failure.
 */
struct wsm_toml *wsm_toml_create(void);

/**
 * @brief Loads a TOML document from a file.
 * @param path Path to the TOML file.
 * @param error Buffer for a human-readable error message, or NULL.
 * @param error_size Size of the error buffer in bytes.
 * @return Loaded TOML document, or NULL on failure.
 */
struct wsm_toml *wsm_toml_load(
	const char *path, char *error, size_t error_size);

/**
 * @brief Destroys a TOML document and all entries it owns.
 * @param toml TOML document to destroy. NULL is accepted.
 */
void wsm_toml_destroy(struct wsm_toml *toml);

/**
 * @brief Checks whether a key exists in the document.
 * @param toml TOML document to search.
 * @param key Dotted key to find.
 * @return true if the key exists, false otherwise.
 */
bool wsm_toml_has(const struct wsm_toml *toml, const char *key);

/**
 * @brief Gets the raw entry for a key.
 * @param toml TOML document to search.
 * @param key Dotted key to find.
 * @return Matching entry, or NULL if not found.
 */
const struct wsm_toml_entry *wsm_toml_get_entry(
	const struct wsm_toml *toml, const char *key);

/**
 * @brief Gets a string value by key.
 * @param toml TOML document to search.
 * @param key Dotted key to read.
 * @param value Pointer to receive a newly allocated string.
 * @return true on success, false if missing, wrong type, or allocation fails.
 * @note The caller owns the returned string and must free it.
 */
bool wsm_toml_get_string(
	const struct wsm_toml *toml, const char *key, char **value);

/**
 * @brief Gets an integer value by key.
 * @param toml TOML document to search.
 * @param key Dotted key to read.
 * @param value Pointer to receive the integer value.
 * @return true on success, false if missing or wrong type.
 */
bool wsm_toml_get_int(
	const struct wsm_toml *toml, const char *key, int64_t *value);

/**
 * @brief Gets a double value by key.
 * @param toml TOML document to search.
 * @param key Dotted key to read.
 * @param value Pointer to receive the floating point value.
 * @return true on success, false if missing or wrong type.
 */
bool wsm_toml_get_double(
	const struct wsm_toml *toml, const char *key, double *value);

/**
 * @brief Gets a boolean value by key.
 * @param toml TOML document to search.
 * @param key Dotted key to read.
 * @param value Pointer to receive the boolean value.
 * @return true on success, false if missing or wrong type.
 */
bool wsm_toml_get_bool(
	const struct wsm_toml *toml, const char *key, bool *value);

/**
 * @brief Sets a string value.
 * @param toml TOML document to modify.
 * @param key Dotted key to set.
 * @param value String value to copy into the document.
 * @return true on success, false on invalid input or allocation failure.
 */
bool wsm_toml_set_string(
	struct wsm_toml *toml, const char *key, const char *value);

/**
 * @brief Sets an integer value.
 * @param toml TOML document to modify.
 * @param key Dotted key to set.
 * @param value Integer value to store.
 * @return true on success, false on invalid input or allocation failure.
 */
bool wsm_toml_set_int(struct wsm_toml *toml, const char *key, int64_t value);

/**
 * @brief Sets a double value.
 * @param toml TOML document to modify.
 * @param key Dotted key to set.
 * @param value Floating point value to store.
 * @return true on success, false on invalid input or allocation failure.
 */
bool wsm_toml_set_double(struct wsm_toml *toml, const char *key, double value);

/**
 * @brief Sets a boolean value.
 * @param toml TOML document to modify.
 * @param key Dotted key to set.
 * @param value Boolean value to store.
 * @return true on success, false on invalid input or allocation failure.
 */
bool wsm_toml_set_bool(struct wsm_toml *toml, const char *key, bool value);

/**
 * @brief Removes a key from the document.
 * @param toml TOML document to modify.
 * @param key Dotted key to remove.
 * @return true if an entry was removed, false if the key was not found or input is invalid.
 */
bool wsm_toml_remove(struct wsm_toml *toml, const char *key);

/**
 * @brief Saves the TOML document to a file atomically.
 * @param toml TOML document to serialize.
 * @param path Destination file path.
 * @return true on success, false on I/O or serialization failure.
 * @details Writes to a temporary file and renames it over path after flushing.
 */
bool wsm_toml_save(const struct wsm_toml *toml, const char *path);

#endif // UTIL_WSM_TOML_H
