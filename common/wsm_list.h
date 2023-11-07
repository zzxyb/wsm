/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef WSM_LIST_H
#define WSM_LIST_H

struct wsm_list {
    int capacity;
    int length;
    void **items;
};

struct wsm_list *wsm_list_create(void);
void list_free(struct wsm_list *list);
void list_add(struct wsm_list *list, void *item);
void list_insert(struct wsm_list *list, int index, void *item);
void list_del(struct wsm_list *list, int index);
void list_cat(struct wsm_list *list, struct wsm_list *source);
void list_qsort(struct wsm_list *list, int compare(const void *left, const void *right));
int list_seq_find(struct wsm_list *list, int compare(const void *item, const void *cmp_to), const void *cmp_to);
int list_find(struct wsm_list *list, const void *item);
void list_stable_sort(struct wsm_list *list, int compare(const void *a, const void *b));
void list_swap(struct wsm_list *list, int src, int dest);
void list_move_to_end(struct wsm_list *list, void *item);
void list_free_items_and_destroy(struct wsm_list *list);

#endif
