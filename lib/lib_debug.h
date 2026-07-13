#ifndef LIB_DEBUG_H
#define LIB_DEBUG_H

#include <stdint.h>

typedef uint32_t (*lib_debug_writer_t)(const uint8_t *data, uint32_t length);

/* The board supplies a register-owning transport; lib_debug stays portable. */
void lib_debug_set_writer(lib_debug_writer_t writer);
uint32_t lib_debug_write(const uint8_t *data, uint32_t length);

#if defined(DEBUG_ENABLED)
#define DBG_WRITE_BYTES(data, length) ((void)lib_debug_write((data), (length)))
/* text must be a string literal or an array so sizeof includes its terminator. */
#define DBG_WRITE_LITERAL(text)                                                        \
    ((void)lib_debug_write((const uint8_t *)(text),                                    \
                           (uint32_t)(sizeof(text) - UINT32_C(1))))
#else
#define DBG_WRITE_BYTES(data, length) ((void)0)
#define DBG_WRITE_LITERAL(text) ((void)0)
#endif

#endif
