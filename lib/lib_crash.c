#include "lib_crash.h"

#include <stddef.h>
#include <stdatomic.h>

#define LIB_CRASH_MAGIC UINT32_C(0x43525348)

_Static_assert(sizeof(lib_crash_record_t) == 32U,
               "crash record layout must stay stable across builds");

static uint32_t lib_crash_crc32(const lib_crash_record_t *record) {
    const uint8_t *bytes = (const uint8_t *)&record->format_version;
    uint32_t crc = UINT32_MAX;
    uint32_t index;

    for (index = 0U; index < (uint32_t)(offsetof(lib_crash_record_t, integrity_crc32) -
                                        offsetof(lib_crash_record_t, format_version));
         ++index) {
        uint32_t bit;

        crc ^= (uint32_t)bytes[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? UINT32_C(0xEDB88320) : 0U);
        }
    }

    return ~crc;
}

static void lib_crash_commit(lib_crash_record_t *record) {
    record->magic = 0U;
    atomic_signal_fence(memory_order_seq_cst);
    record->format_version = LIB_CRASH_FORMAT_VERSION;
    record->record_size = (uint16_t)sizeof(*record);
    record->integrity_crc32 = lib_crash_crc32(record);
    atomic_signal_fence(memory_order_seq_cst);
    record->magic = LIB_CRASH_MAGIC;
}

static void lib_crash_clear_record(lib_crash_record_t *record) {
    record->magic = 0U;
    record->format_version = 0U;
    record->record_size = 0U;
    record->sequence = 0U;
    record->reset_cause = 0U;
    record->reason = (uint32_t)LIB_CRASH_REASON_NONE;
    record->stacked_pc = 0U;
    record->stacked_xpsr = 0U;
    record->integrity_crc32 = 0U;
}

static uint32_t lib_crash_pack_reason(lib_crash_reason_t reason,
                                      uint32_t exception_number) {
    return ((exception_number & LIB_CRASH_EXCEPTION_NUMBER_MASK)
            << LIB_CRASH_EXCEPTION_NUMBER_SHIFT) |
           ((uint32_t)reason & LIB_CRASH_REASON_CODE_MASK);
}

bool lib_crash_is_valid(const lib_crash_record_t *record) {
    if ((record == NULL) || (record->magic != LIB_CRASH_MAGIC) ||
        (record->format_version != LIB_CRASH_FORMAT_VERSION) ||
        (record->record_size != (uint16_t)sizeof(*record))) {
        return false;
    }

    return record->integrity_crc32 == lib_crash_crc32(record);
}

bool lib_crash_has_fault(const lib_crash_record_t *record) {
    return lib_crash_reason(record) != LIB_CRASH_REASON_NONE;
}

lib_crash_reason_t lib_crash_reason(const lib_crash_record_t *record) {
    if (!lib_crash_is_valid(record)) {
        return LIB_CRASH_REASON_NONE;
    }

    return (lib_crash_reason_t)(record->reason & LIB_CRASH_REASON_CODE_MASK);
}

uint32_t lib_crash_exception_number(const lib_crash_record_t *record) {
    if (!lib_crash_is_valid(record)) {
        return 0U;
    }

    return record->reason >> LIB_CRASH_EXCEPTION_NUMBER_SHIFT;
}

void lib_crash_note_boot(lib_crash_record_t *record, uint32_t reset_cause) {
    if (record == NULL) {
        return;
    }

    if (!lib_crash_is_valid(record)) {
        lib_crash_clear_record(record);
    }

    record->reset_cause = reset_cause;
    lib_crash_commit(record);
}

void lib_crash_capture(lib_crash_record_t *record, lib_crash_reason_t reason,
                       uint32_t exception_number, uint32_t stacked_pc,
                       uint32_t stacked_xpsr) {
    uint32_t sequence = 0U;
    uint32_t reset_cause = 0U;

    if (record == NULL) {
        return;
    }

    if (lib_crash_is_valid(record)) {
        sequence = record->sequence;
        reset_cause = record->reset_cause;
    }

    lib_crash_clear_record(record);
    record->sequence = sequence + UINT32_C(1);
    record->reset_cause = reset_cause;
    record->reason = lib_crash_pack_reason(reason, exception_number);
    record->stacked_pc = stacked_pc;
    record->stacked_xpsr = stacked_xpsr;
    lib_crash_commit(record);
}
