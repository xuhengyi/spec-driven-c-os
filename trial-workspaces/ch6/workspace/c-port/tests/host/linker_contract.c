#include "common/assert.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rcore/linker.h"

unsigned char __start[1];
unsigned char __rodata[1];
unsigned char __data[1];
unsigned char __sbss[1];
unsigned char __ebss[1];
unsigned char __boot[1];
unsigned char __end[1];

extern const unsigned char apps[];
extern const unsigned char __app_meta_start[];
extern const unsigned char __app_meta_end[];
extern const unsigned char app0_payload[];
extern const unsigned char app1_payload[];
extern const unsigned char app1_end[];

__asm__(
    ".pushsection .rodata.linker_contract,\"a\",@progbits\n"
    ".balign 8\n"
    ".globl apps\n"
    ".globl __app_meta_start\n"
    "apps:\n"
    "__app_meta_start:\n"
    "    .quad 0\n"
    "    .quad 0\n"
    "    .quad 2\n"
    "    .quad app0_payload\n"
    "    .quad app1_payload\n"
    "    .quad app1_end\n"
    ".globl __app_meta_end\n"
    "__app_meta_end:\n"
    ".globl app0_payload\n"
    "app0_payload:\n"
    "    .ascii \"abc\"\n"
    ".globl app1_payload\n"
    "app1_payload:\n"
    "    .ascii \"XYZ123\"\n"
    ".globl app1_end\n"
    "app1_end:\n"
    ".popsection\n");

static void test_app_meta_contract(void) {
    struct rcore_app_meta meta0;
    struct rcore_app_meta meta1;

    assert(rcore_app_count() == 2);

    meta0 = rcore_app_get(0);
    assert(meta0.start == (uintptr_t)app0_payload);
    assert(meta0.size == 3);

    meta1 = rcore_app_get(1);
    assert(meta1.start == (uintptr_t)app1_payload);
    assert(meta1.size == (size_t)(app1_end - app1_payload));
    assert(meta1.size == 6);
}

static void test_iterator_slot_copy_and_zero_fill(void) {
    enum { SLOT_SIZE = 32 };
    unsigned char slots[SLOT_SIZE * 2];
    struct rcore_app_iterator iter;
    struct rcore_app_meta meta;

    memset(slots, 0x55, sizeof(slots));
    rcore_app_iterator_init(&iter, (uintptr_t)slots, SLOT_SIZE);

    assert(rcore_app_iterator_has_next(&iter));
    assert(rcore_app_iterator_load_next(&iter, &meta));
    assert(meta.start == (uintptr_t)app0_payload);
    assert(meta.size == 3);
    assert(memcmp(slots, "abc", 3) == 0);
    assert(slots[3] == 0);

    assert(rcore_app_iterator_has_next(&iter));
    assert(rcore_app_iterator_load_next(&iter, &meta));
    assert(meta.start == (uintptr_t)app1_payload);
    assert(meta.size == 6);
    assert(memcmp(slots + SLOT_SIZE, "XYZ123", 6) == 0);
    assert(slots[SLOT_SIZE + 6] == 0);

    assert(!rcore_app_iterator_has_next(&iter));
    assert(!rcore_app_iterator_load_next(&iter, NULL));
}

int main(void) {
    test_app_meta_contract();
    test_iterator_slot_copy_and_zero_fill();
    return 0;
}
