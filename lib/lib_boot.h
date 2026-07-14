#ifndef LIB_BOOT_H
#define LIB_BOOT_H

#include "lib_buildinfo.h"

/* Emits a deterministic debug banner through the register-free lib_debug path. */
void lib_boot_write_banner(const char *application_name,
                           const lib_buildinfo_t *buildinfo);

#if defined(DEBUG_ENABLED)
#define LIB_BOOT_BANNER(application_name, buildinfo)                                   \
    lib_boot_write_banner((application_name), (buildinfo))
#else
#define LIB_BOOT_BANNER(application_name, buildinfo) ((void)0)
#endif

#endif
