#ifndef RCORE_HOST_ASSERT_H
#define RCORE_HOST_ASSERT_H

#include <assert.h>
#include <stdio.h>

static inline void rcore_contract_pending(const char *spec_id) {
    fprintf(stderr, "contract pending: %s\n", spec_id);
}

#endif
