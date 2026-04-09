#ifndef RCORE_LINKER_APP_H
#define RCORE_LINKER_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rcore_app_meta {
    uintptr_t start;
    size_t size;
};

struct rcore_app_iterator {
    size_t index;
    size_t total;
    uintptr_t slot_base;
    size_t slot_size;
};

size_t rcore_app_count(void);
struct rcore_app_meta rcore_app_get(size_t index);
bool rcore_app_meta_valid(const struct rcore_app_meta *meta);
void rcore_app_iterator_init(struct rcore_app_iterator *iter, uintptr_t slot_base, size_t slot_size);
bool rcore_app_iterator_has_next(const struct rcore_app_iterator *iter);
bool rcore_app_iterator_load_next(struct rcore_app_iterator *iter, struct rcore_app_meta *meta_out);

#endif
