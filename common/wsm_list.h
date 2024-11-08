#ifndef WSM_LIST_H
#define WSM_LIST_H

/**
 * @brief Structure representing a dynamic list.
 */
struct wsm_list {
	void **items; /**< Pointer to the array of items in the list */
	int capacity; /**< Maximum number of items the list can hold */
	int length; /**< Current number of items in the list */
};

/**
 * @brief Creates a new dynamic list.
 * @return Pointer to the newly created wsm_list instance.
 */
struct wsm_list *wsm_list_create(void);

/**
 * @brief Frees the memory allocated for the specified list.
 * @param list Pointer to the wsm_list instance to free.
 */
void wsm_list_destroy(struct wsm_list *list);

/**
 * @brief Adds an item to the end of the specified list.
 * @param list Pointer to the wsm_list instance.
 * @param item Pointer to the item to add.
 */
void wsm_list_add(struct wsm_list *list, void *item);

/**
 * @brief Inserts an item at the specified index in the list.
 * @param list Pointer to the wsm_list instance.
 * @param index Index at which to insert the item.
 * @param item Pointer to the item to insert.
 */
void wsm_list_insert(struct wsm_list *list, int index, void *item);

/**
 * @brief Deletes an item at the specified index from the list.
 * @param list Pointer to the wsm_list instance.
 * @param index Index of the item to delete.
 */
void wsm_list_delete(struct wsm_list *list, int index);

/**
 * @brief Concatenates another list to the end of the specified list.
 * @param list Pointer to the wsm_list instance.
 * @param source Pointer to the source wsm_list to concatenate.
 */
void wsm_list_cat(struct wsm_list *list, struct wsm_list *source);

/**
 * @brief Sorts the items in the list using the specified comparison function.
 * @param list Pointer to the wsm_list instance.
 * @param compare Comparison function to determine the order of items.
 */
void wsm_list_qsort(struct wsm_list *list,
	int compare(const void *left, const void *right));

/**
 * @brief Sequentially finds an item in the list using the specified comparison function.
 * @param list Pointer to the wsm_list instance.
 * @param compare Comparison function to compare items.
 * @param cmp_to Pointer to the item to compare against.
 * @return Index of the found item, or -1 if not found.
 */
int wsm_list_seq_find(struct wsm_list *list,
	int compare(const void *item, const void *cmp_to), const void *cmp_to);

/**
 * @brief Finds the index of the specified item in the list.
 * @param list Pointer to the wsm_list instance.
 * @param item Pointer to the item to find.
 * @return Index of the found item, or -1 if not found.
 */
int wsm_list_find(struct wsm_list *list, const void *item);

/**
 * @brief Performs a stable sort on the items in the list using the specified comparison function.
 * @param list Pointer to the wsm_list instance.
 * @param compare Comparison function to determine the order of items.
 */
void wsm_list_stable_sort(struct wsm_list *list,
	int compare(const void *a, const void *b));

/**
 * @brief Swaps two items in the list at the specified indices.
 * @param list Pointer to the wsm_list instance.
 * @param src Index of the first item to swap.
 * @param dest Index of the second item to swap.
 */
void wsm_list_swap(struct wsm_list *list, int src, int dest);

/**
 * @brief Moves the specified item to the end of the list.
 * @param list Pointer to the wsm_list instance.
 * @param item Pointer to the item to move.
 */
void wsm_list_move_to_end(struct wsm_list *list, void *item);

/**
 * @brief Frees the items in the list and destroys the list itself.
 * @param list Pointer to the wsm_list instance to free.
 */
void wsm_list_free_items_and_destroy(struct wsm_list *list);

#endif
