// dlist.h
#ifndef DLIST_H
#define DLIST_H

#include "sys/types.h"

typedef struct dlist_node {
    struct dlist_node *prev;
    struct dlist_node *next;
} dlist_node_t;

typedef struct {
    dlist_node_t head;
} dlist_t;

// Initialize a list head
static inline void dlist_init(dlist_t *list) {
    list->head.prev = &list->head;
    list->head.next = &list->head;
}

// Initialize a node (useful for detached nodes)
static inline void dlist_node_init(dlist_node_t *node) {
    node->prev = node;
    node->next = node;
}

// Insert node after position
static inline void dlist_insert_after(dlist_node_t *pos, dlist_node_t *node) {
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
}

// Insert node before position
static inline void dlist_insert_before(dlist_node_t *pos, dlist_node_t *node) {
    node->prev = pos->prev;
    node->next = pos;
    pos->prev->next = node;
    pos->prev = node;
}

// Add to front of list
static inline void dlist_push_front(dlist_t *list, dlist_node_t *node) {
    dlist_insert_after(&list->head, node);
}

// Add to back of list
static inline void dlist_push_back(dlist_t *list, dlist_node_t *node) {
    dlist_insert_before(&list->head, node);
}

// Remove a node from the list
static inline void dlist_remove(dlist_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node;
    node->next = node;
}

// Check if list is empty
static inline int dlist_empty(dlist_t *list) {
    return list->head.next == &list->head;
}

// Get the containing structure from a node pointer
#define dlist_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Iterate over list
#define dlist_for_each(pos, list) \
    for (pos = (list)->head.next; pos != &(list)->head; pos = pos->next)

// Iterate over list (safe against removal)
#define dlist_for_each_safe(pos, tmp, list) \
    for (pos = (list)->head.next, tmp = pos->next; \
         pos != &(list)->head; \
         pos = tmp, tmp = pos->next)

// Iterate over entries
#define dlist_for_each_entry(pos, list, type, member) \
    for (pos = dlist_entry((list)->head.next, type, member); \
         &pos->member != &(list)->head; \
         pos = dlist_entry(pos->member.next, type, member))

#endif