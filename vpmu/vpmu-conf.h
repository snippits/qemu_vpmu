#ifndef __VPMU_CONF_H_
#define __VPMU_CONF_H_
// clang-format off

#define VPMU_MAX_CPU_CORES 10
#define VPMU_MAX_GPU_CORES 4
#define VPMU_MAX_NUM_WORKERS 16

#define LINUX_NAMELEN 16

#define SNIPPIT_JSON_API_VERSION "1.0"

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
#define STR_PHASE  BASH_COLOR_GREEN "[PHASE]" BASH_COLOR_NONE "     "
#define STR_PROC   BASH_COLOR_GREEN "[PROCESS]" BASH_COLOR_NONE "   "
#define STR_KERNEL BASH_COLOR_GREEN "[KERNEL]" BASH_COLOR_NONE "    "

// clang-format on
#endif
