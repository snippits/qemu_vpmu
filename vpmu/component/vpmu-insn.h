#ifndef __VPMU_INSN_H_
#define __VPMU_INSN_H_

#include "config-target.h"   // Target Configuration (CONFIG_ARM)
#include "../vpmu-conf.h"    // VPMU_MAX_CPU_CORES
#include "../vpmu-common.h"  // Include common headers
#include "../vpmu-extratb.h" // Extra TB Information

uint64_t vpmu_total_insn_count(void);
void vpmu_insn_ref(uint8_t core, uint8_t mode, ExtraTBInfo *ptr);

#endif
