#ifndef RCORE_LINKER_H
#define RCORE_LINKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct rcore_kernel_layout {
    uintptr_t text;
    uintptr_t rodata;
    uintptr_t data;
    uintptr_t sbss;
    uintptr_t ebss;
    uintptr_t boot;
    uintptr_t end;
};

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

const char *rcore_linker_script(void);
struct rcore_kernel_layout rcore_kernel_layout_locate(void);
void rcore_zero_bss(const struct rcore_kernel_layout *layout);
size_t rcore_app_count(void);
struct rcore_app_meta rcore_app_get(size_t index);
bool rcore_app_meta_valid(const struct rcore_app_meta *meta);
void rcore_app_iterator_init(struct rcore_app_iterator *iter, uintptr_t slot_base, size_t slot_size);
bool rcore_app_iterator_has_next(const struct rcore_app_iterator *iter);
bool rcore_app_iterator_load_next(struct rcore_app_iterator *iter, struct rcore_app_meta *meta_out);

#endif
