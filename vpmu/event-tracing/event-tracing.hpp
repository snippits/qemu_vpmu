#ifndef __VPMU_EVENT_TRACING_HPP_
#define __VPMU_EVENT_TRACING_HPP_
#pragma once

extern "C" {
#include "config-target.h" // Target configuration
#include "vpmu-common.h"   // Common headers and macros
#include "event-tracing.h" // C header
#include "vpmu.h"          // vpmu_clone_qemu_cpu_state
}

#include <string>          // std::string
#include <vector>          // std::vector
#include <utility>         // std::forward
#include <map>             // std::map
#include <algorithm>       // std::remove_if
#include <mutex>           // Mutex
#include "vpmu-log.hpp"    // Log system
#include "vpmu-utils.hpp"  // Misc. functions
#include "phase/phase.hpp" // Phase class
#include "et-path.hpp"     // ET_Path class
#include "et-program.hpp"  // ET_Program class
#include "et-process.hpp"  // ET_Process class
#include "et-kernel.hpp"   // ET_Kernel class

#include "vpmu-snapshot.hpp" // VPMUSanpshot

// TODO Use weak_ptr to implement a use_count() tester to check
// if all programs, processes are free normally

class EventTracer : VPMULog
{
public:
    EventTracer() : VPMULog("ET") {}
    EventTracer(const char* module_name) : VPMULog(module_name) {}
    EventTracer(std::string module_name) : VPMULog(module_name) {}
    // EventTracer is neither copyable nor movable.
    EventTracer(const EventTracer&) = delete;
    EventTracer& operator=(const EventTracer&) = delete;

    std::shared_ptr<ET_Program> add_program(std::string name);
    std::shared_ptr<ET_Program> add_library(std::string name);
    std::shared_ptr<ET_Program> find_program(const char* path);
    void remove_program(std::string name);
    void remove_process(uint64_t pid);
    void clear_shared_libraries(void);
    void update_elf_dwarf(std::shared_ptr<ET_Program>& program, const char* file_name);

    void attach_mapped_region(std::shared_ptr<ET_Process>& process, MMapInfo mmap_info);

    inline std::shared_ptr<ET_Process> add_new_process(const char* name, uint64_t pid)
    {
        // Lock when updating the process_id_map (thread shared resource)
        std::lock_guard<std::mutex> lock(process_id_map_lock);

        auto program = find_program(name);
        // Check if the target program is in the monitoring list
        if (program != nullptr) {
            log_debug("Trace new process %s, pid:%5" PRIu64, name, pid);
            auto&& process      = std::make_shared<ET_Process>(program, pid);
            process->name       = name;
            process_id_map[pid] = process;
            debug_dump_process_map();
            return process;
        } else {
            log_debug("Trace new process %s, pid:%5" PRIu64, name, pid);
            auto&& process      = std::make_shared<ET_Process>(name, pid);
            process->name       = name;
            process_id_map[pid] = process;
            debug_dump_process_map();
            return process;
        }
        return nullptr;
    }

    inline std::shared_ptr<ET_Process> find_process(uint64_t pid)
    {
        if (pid == 0) return nullptr; // pid should never be 0
        for (auto& p_pair : process_id_map) {
            auto& p = p_pair.second;
            if (p->pid == pid) return p;
        }
        return nullptr;
    }

    inline std::shared_ptr<ET_Process> find_process(const char* path)
    {
        if (path == nullptr) return nullptr;

        for (auto& p_pair : process_id_map) {
            auto& p = p_pair.second;
            if (p->fuzzy_compare_name(path)) return p;
        }
        return nullptr;
    }

    inline void attach_to_parent(std::shared_ptr<ET_Process> parent, uint64_t child_pid)
    {
        if (parent == nullptr) return;
        // Fork a new child process
        auto process = std::make_shared<ET_Process>(*parent, child_pid);
        log_debug("Attach process %5" PRIu64 " to %5" PRIu64, child_pid, parent->pid);
        {
            std::lock_guard<std::mutex> lock(process_lock);
            parent->push_child_process(process);
        }
        {
            std::lock_guard<std::mutex> lock(process_id_map_lock);
            process_id_map[child_pid] = process;
        }
        // debug_dump_process_map();
    }

    inline void attach_to_program(std::shared_ptr<ET_Program>  target_program,
                                  std::shared_ptr<ET_Program>& program)
    {
        if (target_program == nullptr) return;
        std::lock_guard<std::mutex> lock(program_list_lock);

        log_debug("Attach program '%s' to binary '%s'",
                  program->name.c_str(),
                  target_program->name.c_str());
        target_program->push_binary(program);
        // debug_dump_process_map();
    }

    ET_Kernel& get_kernel(void) { return kernel; }

    // Return 0 when parse fail, return linux version number when succeed
    uint64_t parse_and_set_kernel_symbol(const char* filename);

    void debug_dump_process_map(void);
    void debug_dump_process_map(std::shared_ptr<ET_Process> marked_process);
    void debug_dump_program_map(void);
    void debug_dump_program_map(std::shared_ptr<ET_Program> marked_program);
    void debug_dump_child_list(const ET_Process& process);
    void debug_dump_binary_list(const ET_Process& process);
    void debug_dump_library_list(const ET_Program& program);

public:
    FunctionMap<std::string, void*, ET_Process*> func_callbacks;

private:
    ET_Kernel kernel;
    std::map<uint64_t, std::shared_ptr<ET_Process>> process_id_map;
    std::vector<std::shared_ptr<ET_Program>> program_list;
    // This mutex protects: process_id_map
    std::mutex process_id_map_lock;
    // This mutex protects: process
    std::mutex process_lock;
    // This mutex protects: program_list
    std::mutex program_list_lock;
};

extern EventTracer event_tracer;

#endif // __VPMU_EVENT_TRACING_HPP_
