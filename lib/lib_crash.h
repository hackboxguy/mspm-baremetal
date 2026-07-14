#ifndef LIB_CRASH_H
#define LIB_CRASH_H

#include <stdbool.h>
#include <stdint.h>

#define LIB_CRASH_FORMAT_VERSION UINT16_C(2)

/* Keep these plain decimal tokens so the naked assembly handlers can use them. */
#define LIB_CRASH_REASON_CODE_NONE 0
#define LIB_CRASH_REASON_CODE_NMI 1
#define LIB_CRASH_REASON_CODE_HARDFAULT 2
#define LIB_CRASH_REASON_CODE_UNEXPECTED_EXCEPTION 3

#define LIB_CRASH_REASON_CODE_MASK UINT32_C(0x000000ff)
#define LIB_CRASH_EXCEPTION_NUMBER_SHIFT UINT32_C(8)
#define LIB_CRASH_EXCEPTION_NUMBER_MASK UINT32_C(0x00ffffff)

typedef enum {
    LIB_CRASH_REASON_NONE = LIB_CRASH_REASON_CODE_NONE,
    LIB_CRASH_REASON_NMI = LIB_CRASH_REASON_CODE_NMI,
    LIB_CRASH_REASON_HARDFAULT = LIB_CRASH_REASON_CODE_HARDFAULT,
    LIB_CRASH_REASON_UNEXPECTED_EXCEPTION = LIB_CRASH_REASON_CODE_UNEXPECTED_EXCEPTION,
} lib_crash_reason_t;

_Static_assert(LIB_CRASH_REASON_NMI == LIB_CRASH_REASON_CODE_NMI,
               "NMI assembly reason code must match the crash enum");
_Static_assert(LIB_CRASH_REASON_HARDFAULT == LIB_CRASH_REASON_CODE_HARDFAULT,
               "HardFault assembly reason code must match the crash enum");
_Static_assert(LIB_CRASH_REASON_UNEXPECTED_EXCEPTION ==
                   LIB_CRASH_REASON_CODE_UNEXPECTED_EXCEPTION,
               "Default assembly reason code must match the crash enum");

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
lib_crash_reason_t lib_crash_reason(const lib_crash_record_t *record);
uint32_t lib_crash_exception_number(const lib_crash_record_t *record);
void lib_crash_note_boot(lib_crash_record_t *record, uint32_t reset_cause);
void lib_crash_capture(lib_crash_record_t *record, lib_crash_reason_t reason,
                       uint32_t exception_number, uint32_t stacked_pc,
                       uint32_t stacked_xpsr);

#endif
