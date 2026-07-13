#ifndef LIB_CRASH_H
#define LIB_CRASH_H

#include <stdbool.h>
#include <stdint.h>

#define LIB_CRASH_FORMAT_VERSION UINT16_C(1)

typedef enum {
    LIB_CRASH_REASON_NONE = 0U,
    LIB_CRASH_REASON_NMI,
    LIB_CRASH_REASON_HARDFAULT,
    LIB_CRASH_REASON_UNEXPECTED_EXCEPTION,
} lib_crash_reason_t;

/* Fixed-width, CRC-protected record suitable for caller-owned .noinit storage. */
typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t record_size;
    uint32_t sequence;
    uint32_t reset_cause;
    uint32_t reason;
    uint32_t stacked_pc;
    uint32_t stacked_xpsr;
    uint32_t integrity_crc32;
} lib_crash_record_t;

bool lib_crash_is_valid(const lib_crash_record_t *record);
bool lib_crash_has_fault(const lib_crash_record_t *record);
void lib_crash_note_boot(lib_crash_record_t *record, uint32_t reset_cause);
void lib_crash_capture(lib_crash_record_t *record, lib_crash_reason_t reason,
                       uint32_t stacked_pc, uint32_t stacked_xpsr);

#endif
