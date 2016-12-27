#ifndef __VPMU_CONF_H_
#define __VPMU_CONF_H_

#define VPMU_MAX_CPU_CORES 64

#define LINUX_NAMELEN 16

#ifndef CONFIG_VPMU_DISABLE_COLOR
#define BASH_COLOR_RED     "\033[0;31m"
#define BASH_COLOR_GREEN   "\033[0;32m"
#define BASH_COLOR_YELLOW  "\033[0;33m"
#define BASH_COLOR_BLUE    "\033[0;34m"
#define BASH_COLOR_PURPLE  "\033[0;35m"
#define BASH_COLOR_CYAN    "\033[0;36m"
#define BASH_COLOR_NONE    "\033[0;00m"
#else
#define BASH_COLOR_RED
#define BASH_COLOR_GREEN
#define BASH_COLOR_YELLOW
#define BASH_COLOR_BLUE
#define BASH_COLOR_PURPLE
#define BASH_COLOR_CYAN
#define BASH_COLOR_NONE

#endif

#define LOG_PREFIX_LEN 10
#define LOG_PREFIX_FORMAT BASH_COLOR_GREEN "[%s]" BASH_COLOR_NONE

#define STR_VPMU   BASH_COLOR_GREEN "[VPMU]" BASH_COLOR_NONE "      "
#define STR_SET    BASH_COLOR_GREEN "[SET]" BASH_COLOR_NONE "       "

#ifndef CONFIG_SOFTMMU
// TODO remove this?
// Only include this QEMU config when config-target.h is not included
#include "config-host.h"
#endif

#endif
