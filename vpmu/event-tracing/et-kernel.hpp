#ifndef __VPMU_ET_KERNEL_HPP_
#define __VPMU_ET_KERNEL_HPP_
#pragma once

extern "C" {
#include "kernel-event-cb.h"      // Kernel related header
#include "event-tracing-helper.h" // et_get_ret_addr()
}

#include <boost/algorithm/string.hpp> // boost::algorithm::to_lower

#include "vpmu.hpp"       // for vpmu-qemu.h and VPMU struct
#include "vpmu-utils.hpp" // miscellaneous functions
#include "et-program.hpp" // ET_Program class

class ET_Kernel : public ET_Program
{
public:
    ET_Kernel() : ET_Program("kernel") {}

    bool check_all_events_set(void)
    {
        for (int i = 0; i < ET_KERNEL_EVENT_COUNT; i++) {
            if (kernel_event_table[i] == 0) {
                return false;
            }
        }
        return true;
    }

    ET_KERNEL_EVENT_TYPE find_event(uint64_t vaddr)
    {
        for (int i = 0; i < ET_KERNEL_EVENT_COUNT; i++) {
            if (kernel_event_table[i] == vaddr) {
                // DBG(STR_VPMU "Found event-%d \n",i);
                return (ET_KERNEL_EVENT_TYPE)i;
            }
        }
        return ET_KERNEL_NONE;
    }

    bool call_event(void* env, uint64_t core_id, uint64_t vaddr)
    {
        if (functions.call(vaddr, env)) {
            functions.update_return_key(vaddr, et_get_ret_addr(env));
            return true;
        } else if (functions.call_return(vaddr, env)) {
            return true;
        }
        return false;
    }

    uint64_t find_vaddr(ET_KERNEL_EVENT_TYPE event)
    {
        if (event < ET_KERNEL_EVENT_COUNT)
            return kernel_event_table[event];
        else
            return 0;
    }

    void set_event_address(ET_KERNEL_EVENT_TYPE event, uint64_t address)
    {
        kernel_event_table[event] = address;
        functions.register_all(address, // Set up all callbacks to the target address
                               events[event].pre_call,
                               events[event].on_call,
                               events[event].on_return);
    }

    bool set_symbol_address(std::string sym_name, uint64_t address)
    {
        // NOTE: The following functions are core functions for these events.
        // We use these instead of system calls because some other system calls
        // might also trigger events of others.
        // Ex: In mmap syscall, i.e. mmap_region(), unmap_region is called in order to
        // undo any partial mapping done by a device driver.
        // If one uses system calls instead of these functions,
        // all mechanisms should still work..... well, in most cases. :P
        // Linux system call series functions might be either sys_XXXX or SyS_XXXX
        boost::algorithm::to_lower(sym_name);
        if (sym_name.find("do_execveat_common") != std::string::npos
            || sym_name.find("do_execve_common") != std::string::npos) {
            set_event_address(ET_KERNEL_EXECV, address);
        } else if (sym_name == "__switch_to") {
            set_event_address(ET_KERNEL_CONTEXT_SWITCH, address);
        } else if (sym_name == "do_exit") {
            set_event_address(ET_KERNEL_EXIT, address);
        } else if (sym_name == "wake_up_new_task") {
            set_event_address(ET_KERNEL_WAKE_NEW_TASK, address);
        } else if (sym_name == "_do_fork" || sym_name == "do_fork") {
            set_event_address(ET_KERNEL_FORK, address);
        } else if (sym_name == "mmap_region") {
            set_event_address(ET_KERNEL_MMAP, address);
        } else if (sym_name == "mprotect_fixup") {
            set_event_address(ET_KERNEL_MPROTECT, address);
        } else if (sym_name == "unmap_region") {
            set_event_address(ET_KERNEL_MUNMAP, address);
        } else {
            return false; // Not Found
        }
        DBG(STR_VPMU "Set Linux symbol %s @ %lx\n", sym_name.c_str(), address);

        return true; // Found
    }

    uint64_t get_running_pid() { return VPMU.core[vpmu::get_core_id()].current_pid; }

    uint64_t get_running_pid(uint64_t core_id)
    {
        return VPMU.core[vpmu::get_core_id()].current_pid;
    }

public:
    /// This is callback function table registered to kernel events
    FunctionMap<enum ET_KERNEL_EVENT_TYPE, void*> events;

private:
    /// This is only used for checking what critical events are not set
    uint64_t kernel_event_table[ET_KERNEL_EVENT_SIZE] = {0};
    /// This is callback functions of kernel function calls (PC addresses)
    FunctionMap<uint64_t, void*> functions;
};

#endif
