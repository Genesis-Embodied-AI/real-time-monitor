#ifndef RTM_LIB_OS_TYPES_H
#define RTM_LIB_OS_TYPES_H

#if defined(__unix__)
#include "posix/os_types.h"
#else
#error "Operating system not supported"
#endif

#endif
