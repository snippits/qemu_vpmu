#ifndef __VPMU_H_
#define __VPMU_H_

#include "config-target.h" // QEMU Target Information
#include "vpmu-common.h"   // Common headers and macros
#include "vpmu-conf.h"     // Import the common configurations and QEMU config-host.h
#include "vpmu-qemu.h"     // The interface between QEMU and VPMU
#include "vpmu-log.h"      // The logging macros of VPMU

//==========================  VPMU Macros  ===========================
//==================  VPMU Externs(Outer Variables) ==================
//========================  VPMU Definitions  ========================

uintptr_t vpmu_get_phy_addr_global(void *ptr, uintptr_t vaddr);
size_t vpmu_copy_from_guest(void *dst, uintptr_t src, const size_t size, void *cs);
void vpmu_dump_elf_symbols(const char *file_path);
void *vpmu_clone_qemu_cpu_state(void *cpu_v);

// Prevent prototype warnings from some compilers
uint8_t *vpmu_read_ptr_from_guest(void *cs, uint64_t addr, uint64_t offset);
uint8_t vpmu_read_uint8_from_guest(void *cs, uint64_t addr, uint64_t offset);
uint16_t vpmu_read_uint16_from_guest(void *cs, uint64_t addr, uint64_t offset);
uint32_t vpmu_read_uint32_from_guest(void *cs, uint64_t addr, uint64_t offset);
uint64_t vpmu_read_uint64_from_guest(void *cs, uint64_t addr, uint64_t offset);
uintptr_t vpmu_read_uintptr_from_guest(void *cs, uint64_t addr, uint64_t offset);

#endif
