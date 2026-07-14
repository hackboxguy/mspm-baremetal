#include "lib_buildinfo.h"

#include <stddef.h>

bool lib_buildinfo_is_bcd(uint8_t value) {
    return ((value & UINT8_C(0x0f)) <= UINT8_C(9)) && ((value >> 4U) <= UINT8_C(9));
}

bool lib_buildinfo_has_timestamp(const lib_buildinfo_t *buildinfo) {
    if (buildinfo == NULL) {
        return false;
    }

    return (buildinfo->year_high != 0U) || (buildinfo->year_low != 0U) ||
           (buildinfo->month != 0U) || (buildinfo->day != 0U) ||
           (buildinfo->hour != 0U) || (buildinfo->minute != 0U);
}

bool lib_buildinfo_is_valid(const lib_buildinfo_t *buildinfo) {
    if ((buildinfo == NULL) || !lib_buildinfo_is_bcd(buildinfo->version_major) ||
        !lib_buildinfo_is_bcd(buildinfo->version_minor)) {
        return false;
    }

    if (!lib_buildinfo_has_timestamp(buildinfo)) {
        return true;
    }

    return lib_buildinfo_is_bcd(buildinfo->year_high) &&
           lib_buildinfo_is_bcd(buildinfo->year_low) &&
           lib_buildinfo_is_bcd(buildinfo->month) &&
           lib_buildinfo_is_bcd(buildinfo->day) &&
           lib_buildinfo_is_bcd(buildinfo->hour) &&
           lib_buildinfo_is_bcd(buildinfo->minute) &&
           (buildinfo->month >= UINT8_C(0x01)) && (buildinfo->month <= UINT8_C(0x12)) &&
           (buildinfo->day >= UINT8_C(0x01)) && (buildinfo->day <= UINT8_C(0x31)) &&
           (buildinfo->hour <= UINT8_C(0x23)) && (buildinfo->minute <= UINT8_C(0x59));
}

bool lib_buildinfo_write_device_info(const lib_buildinfo_t *buildinfo, uint8_t *bytes,
                                     uint32_t length) {
    if (!lib_buildinfo_is_valid(buildinfo) || (bytes == NULL) ||
        (length < LIB_BUILDINFO_DEVICE_INFO_SIZE)) {
        return false;
    }

    bytes[0] = buildinfo->version_major;
    bytes[1] = buildinfo->version_minor;
    bytes[2] = buildinfo->year_high;
    bytes[3] = buildinfo->year_low;
    bytes[4] = buildinfo->month;
    bytes[5] = buildinfo->day;
    bytes[6] = buildinfo->hour;
    bytes[7] = buildinfo->minute;
    return true;
}
