/* Local execution context helpers used by the batch kernel. */
#ifndef _HOME_XU_HY22_GRADUATION_PHASE_C_CANDIDATE_TEMPLATE_C_PORT_INCLUDE_RCORE_KERNEL_CONTEXT_LOCAL_H_
#define _HOME_XU_HY22_GRADUATION_PHASE_C_CANDIDATE_TEMPLATE_C_PORT_INCLUDE_RCORE_KERNEL_CONTEXT_LOCAL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rcore_local_context {
    uintptr_t sctx;
    uintptr_t x[31];
    uintptr_t sepc;
    bool supervisor;
    bool interrupt;
};

struct rcore_local_context rcore_local_context_empty(void);
struct rcore_local_context rcore_local_context_user(uintptr_t pc);
void rcore_local_context_set_sp(struct rcore_local_context *ctx, uintptr_t value);
uintptr_t rcore_local_context_get_a(const struct rcore_local_context *ctx, size_t index);
void rcore_local_context_set_a(struct rcore_local_context *ctx, size_t index, uintptr_t value);
void rcore_local_context_move_next(struct rcore_local_context *ctx);
uintptr_t rcore_local_context_execute(struct rcore_local_context *ctx);

#endif /* _HOME_XU_HY22_GRADUATION_PHASE_C_CANDIDATE_TEMPLATE_C_PORT_INCLUDE_RCORE_KERNEL_CONTEXT_LOCAL_H_ */
