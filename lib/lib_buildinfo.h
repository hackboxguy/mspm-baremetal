#ifndef LIB_BUILDINFO_H
#define LIB_BUILDINFO_H

#include <stdbool.h>
#include <stdint.h>

#define LIB_BUILDINFO_DEVICE_INFO_SIZE UINT32_C(8)

/* The all-zero date/time denotes a reproducible build with no supplied time. */
typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t year_high;
    uint8_t year_low;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
} lib_buildinfo_t;

bool lib_buildinfo_is_bcd(uint8_t value);
bool lib_buildinfo_has_timestamp(const lib_buildinfo_t *buildinfo);
bool lib_buildinfo_is_valid(const lib_buildinfo_t *buildinfo);
bool lib_buildinfo_write_device_info(const lib_buildinfo_t *buildinfo, uint8_t *bytes,
                                     uint32_t length);

#endif
