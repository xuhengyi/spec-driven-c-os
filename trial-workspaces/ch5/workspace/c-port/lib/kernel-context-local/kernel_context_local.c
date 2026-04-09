#include "rcore/kernel_context_local.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RCORE_SSTATUS_SPP (1UL << 8)
#define RCORE_SSTATUS_SPIE (1UL << 5)

static inline uintptr_t build_sstatus(bool supervisor, bool interrupt) {
    uintptr_t value;
    asm volatile("csrr %0, sstatus" : "=r"(value));
    if (supervisor) {
        value |= RCORE_SSTATUS_SPP;
    } else {
        value &= ~RCORE_SSTATUS_SPP;
    }
    if (interrupt) {
        value |= RCORE_SSTATUS_SPIE;
    } else {
        value &= ~RCORE_SSTATUS_SPIE;
    }
    return value;
}

static inline size_t reg_index(size_t reg) {
    return reg - 1;
}

static inline size_t a_index(size_t index) {
    return reg_index(10 + index);
}

struct rcore_local_context rcore_local_context_empty(void) {
    struct rcore_local_context ctx = {
        .sctx = 0,
        .x = {0},
        .sepc = 0,
        .supervisor = false,
        .interrupt = false,
    };
    return ctx;
}

struct rcore_local_context rcore_local_context_user(uintptr_t pc) {
    struct rcore_local_context ctx = rcore_local_context_empty();
    ctx.sepc = pc;
    ctx.interrupt = true;
    return ctx;
}

void rcore_local_context_set_sp(struct rcore_local_context *ctx, uintptr_t value) {
    ctx->x[reg_index(2)] = value;
}

uintptr_t rcore_local_context_get_a(const struct rcore_local_context *ctx, size_t index) {
    return ctx->x[a_index(index)];
}

void rcore_local_context_set_a(struct rcore_local_context *ctx, size_t index, uintptr_t value) {
    ctx->x[a_index(index)] = value;
}

void rcore_local_context_move_next(struct rcore_local_context *ctx) {
    ctx->sepc += 4;
}

__attribute__((naked)) void rcore_local_context_execute_naked(void) {
    asm volatile(
        "  .altmacro\n"
        "  .macro SAVE n\n"
        "      sd x\\n, \\n*8(sp)\n"
        "  .endm\n"
        "  .macro SAVE_ALL\n"
        "      sd x1, 1*8(sp)\n"
        "      .set n, 3\n"
        "      .rept 29\n"
        "          SAVE %%n\n"
        "          .set n, n+1\n"
        "      .endr\n"
        "  .endm\n"
        "  .macro LOAD n\n"
        "      ld x\\n, \\n*8(sp)\n"
        "  .endm\n"
        "  .macro LOAD_ALL\n"
        "      ld x1, 1*8(sp)\n"
        "      .set n, 3\n"
        "      .rept 29\n"
        "          LOAD %%n\n"
        "          .set n, n+1\n"
        "      .endr\n"
        "  .endm\n"
        "  .option push\n"
        "  .option nopic\n"
        "  addi sp, sp, -32*8\n"
        "  SAVE_ALL\n"
        "  la   t0, 1f\n"
        "  csrw stvec, t0\n"
        "  csrr t0, sscratch\n"
        "  sd   sp, (t0)\n"
        "  mv   sp, t0\n"
        "  LOAD_ALL\n"
        "  ld   sp, 2*8(sp)\n"
        "  sret\n"
        "  .align 2\n"
        "1: csrrw sp, sscratch, sp\n"
        "  SAVE_ALL\n"
        "  csrrw t0, sscratch, sp\n"
        "  sd    t0, 2*8(sp)\n"
        "  ld sp, (sp)\n"
        "  LOAD_ALL\n"
        "  addi sp, sp, 32*8\n"
        "  ret\n"
        "  .option pop\n"
        :
        :
        : "memory");
}

uintptr_t rcore_local_context_execute(struct rcore_local_context *ctx) {
    uintptr_t sstatus = build_sstatus(ctx->supervisor, ctx->interrupt);
    uintptr_t sepc = ctx->sepc;
    uintptr_t old_sscratch;
    uintptr_t ctx_ptr = (uintptr_t)ctx;
    asm volatile(
        "csrrw %0, sscratch, %3\n"
        "csrw sepc, %1\n"
        "csrw sstatus, %2\n"
        "addi sp, sp, -8\n"
        "sd ra, 0(sp)\n"
        "call rcore_local_context_execute_naked\n"
        "ld ra, 0(sp)\n"
        "addi sp, sp, 8\n"
        "csrw sscratch, %0\n"
        "csrr %1, sepc\n"
        "csrr %2, sstatus\n"
        : "=&r"(old_sscratch), "+r"(sepc), "+r"(sstatus)
        : "r"(ctx_ptr)
        : "memory", "ra");
    ctx->sepc = sepc;
    return sstatus;
}
