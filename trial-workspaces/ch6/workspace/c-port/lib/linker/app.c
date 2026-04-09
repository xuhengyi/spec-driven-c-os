#include <string.h>

#include "app.h"

enum {
    RCORE_APP_META_BASE_INDEX = 0,
    RCORE_APP_META_STEP_INDEX = 1,
    RCORE_APP_META_COUNT_INDEX = 2,
    RCORE_APP_META_FIRST_INDEX = 3,
};

static const uintptr_t *rcore_app_meta_words(void) {
    extern const uintptr_t apps[];
    return apps;
}

static size_t rcore_app_meta_declared_count(void) {
    const uintptr_t *words = rcore_app_meta_words();
    size_t declared = (size_t)words[RCORE_APP_META_COUNT_INDEX];
    if (declared == 0) {
        return 0;
    }
    return declared;
}

size_t rcore_app_count(void) {
    return rcore_app_meta_declared_count();
}

struct rcore_app_meta rcore_app_get(size_t index) {
    struct rcore_app_meta invalid = {0, 0};
    size_t count = rcore_app_meta_declared_count();
    if (index >= count) {
        return invalid;
    }
    const uintptr_t *words = rcore_app_meta_words();
    size_t addr_base = RCORE_APP_META_FIRST_INDEX;
    uintptr_t start = words[addr_base + index];
    uintptr_t end = words[addr_base + index + 1];
    if (end <= start) {
        return invalid;
    }
    struct rcore_app_meta meta = {start, (size_t)(end - start)};
    return meta;
}

bool rcore_app_meta_valid(const struct rcore_app_meta *meta) {
    if (!meta) {
        return false;
    }
    return meta->size > 0;
}

void rcore_app_iterator_init(struct rcore_app_iterator *iter,
                             uintptr_t slot_base,
                             size_t slot_size) {
    if (!iter) {
        return;
    }
    iter->index = 0;
    iter->total = rcore_app_count();
    iter->slot_base = slot_base;
    iter->slot_size = slot_size;
}

bool rcore_app_iterator_has_next(const struct rcore_app_iterator *iter) {
    return iter && iter->index < iter->total;
}

static bool rcore_app_iterator_copy_slot(const struct rcore_app_iterator *iter,
                                         const struct rcore_app_meta *meta) {
    if (!iter || iter->slot_size == 0) {
        return true;
    }
    size_t app_size = meta->size;
    if (app_size > iter->slot_size) {
        return false;
    }
    unsigned char *dst =
        (unsigned char *)(iter->slot_base + iter->index * iter->slot_size);
    const unsigned char *src = (const unsigned char *)meta->start;
    if (app_size > 0) {
        memcpy(dst, src, app_size);
    }
    if (iter->slot_size > app_size) {
        memset(dst + app_size, 0, iter->slot_size - app_size);
    }
    return true;
}

bool rcore_app_iterator_load_next(struct rcore_app_iterator *iter,
                                  struct rcore_app_meta *meta_out) {
    if (!rcore_app_iterator_has_next(iter)) {
        return false;
    }
    struct rcore_app_meta meta = rcore_app_get(iter->index);
    if (!rcore_app_meta_valid(&meta)) {
        iter->index = iter->total;
        return false;
    }
    if (!rcore_app_iterator_copy_slot(iter, &meta)) {
        return false;
    }
    if (meta_out) {
        *meta_out = meta;
    }
    iter->index++;
    return true;
}
