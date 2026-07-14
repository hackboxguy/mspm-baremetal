#include <stddef.h>
#include <stdint.h>

/*
 * The firmware is linked without a C library. Keep the compiler-permitted
 * byte-copy helpers owned here so structure copies in portable code never
 * create hidden libc dependencies.
 */
void *memcpy(void *destination, const void *source, size_t length) {
    uint8_t *destination_bytes = destination;
    const uint8_t *source_bytes = source;
    size_t index;

    for (index = 0U; index < length; ++index) {
        destination_bytes[index] = source_bytes[index];
    }
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    uint8_t *destination_bytes = destination;
    size_t index;

    for (index = 0U; index < length; ++index) {
        destination_bytes[index] = (uint8_t)value;
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t length) {
    uint8_t *destination_bytes = destination;
    const uint8_t *source_bytes = source;

    if ((uintptr_t)destination_bytes < (uintptr_t)source_bytes) {
        return memcpy(destination, source, length);
    }

    while (length != 0U) {
        --length;
        destination_bytes[length] = source_bytes[length];
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length) {
    const uint8_t *left_bytes = left;
    const uint8_t *right_bytes = right;
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (left_bytes[index] != right_bytes[index]) {
            return left_bytes[index] < right_bytes[index] ? -1 : 1;
        }
    }
    return 0;
}
