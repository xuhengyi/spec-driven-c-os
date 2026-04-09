#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t len) {
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;
    for (size_t i = 0; i < len; ++i) {
        out[i] = in[i];
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t len) {
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;
    if (out == in || len == 0) {
        return dst;
    }
    if (out < in) {
        for (size_t i = 0; i < len; ++i) {
            out[i] = in[i];
        }
        return dst;
    }
    for (size_t i = len; i > 0; --i) {
        out[i - 1] = in[i - 1];
    }
    return dst;
}

void *memset(void *dst, int value, size_t len) {
    unsigned char *out = (unsigned char *)dst;
    unsigned char byte = (unsigned char)value;
    for (size_t i = 0; i < len; ++i) {
        out[i] = byte;
    }
    return dst;
}

int memcmp(const void *lhs, const void *rhs, size_t len) {
    const unsigned char *left = (const unsigned char *)lhs;
    const unsigned char *right = (const unsigned char *)rhs;
    for (size_t i = 0; i < len; ++i) {
        if (left[i] != right[i]) {
            return (int)left[i] - (int)right[i];
        }
    }
    return 0;
}
