#include "lib_debug.h"

#include <stddef.h>

static lib_debug_writer_t g_writer;

void lib_debug_set_writer(lib_debug_writer_t writer) {
    g_writer = writer;
}

uint32_t lib_debug_write(const uint8_t *data, uint32_t length) {
    if ((g_writer == NULL) || ((data == NULL) && (length != 0U))) {
        return 0U;
    }

    return g_writer(data, length);
}
