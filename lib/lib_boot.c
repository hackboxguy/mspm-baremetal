#include "lib_boot.h"

#include <stddef.h>

#include "lib_debug.h"

static void lib_boot_write_byte(uint8_t value) {
    (void)lib_debug_write(&value, UINT32_C(1));
}

static void lib_boot_write_text(const char *text) {
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        lib_boot_write_byte((uint8_t)*text);
        ++text;
    }
}

static void lib_boot_write_bcd(uint8_t value) {
    lib_boot_write_byte((uint8_t)('0' + (char)(value >> 4U)));
    lib_boot_write_byte((uint8_t)('0' + (char)(value & UINT8_C(0x0f))));
}

void lib_boot_write_banner(const char *application_name,
                           const lib_buildinfo_t *buildinfo) {
    lib_boot_write_text("mspm-baremetal: ");
    lib_boot_write_text(application_name == NULL ? "unnamed" : application_name);

    if (!lib_buildinfo_is_valid(buildinfo)) {
        lib_boot_write_text(" build=invalid\r\n");
        return;
    }

    lib_boot_write_text(" v");
    lib_boot_write_bcd(buildinfo->version_major);
    lib_boot_write_byte((uint8_t)'.');
    lib_boot_write_bcd(buildinfo->version_minor);
    lib_boot_write_text(" build=");

    if (!lib_buildinfo_has_timestamp(buildinfo)) {
        lib_boot_write_text("unspecified\r\n");
        return;
    }

    lib_boot_write_bcd(buildinfo->year_high);
    lib_boot_write_bcd(buildinfo->year_low);
    lib_boot_write_byte((uint8_t)'-');
    lib_boot_write_bcd(buildinfo->month);
    lib_boot_write_byte((uint8_t)'-');
    lib_boot_write_bcd(buildinfo->day);
    lib_boot_write_byte((uint8_t)'T');
    lib_boot_write_bcd(buildinfo->hour);
    lib_boot_write_byte((uint8_t)':');
    lib_boot_write_bcd(buildinfo->minute);
    lib_boot_write_text("Z\r\n");
}
