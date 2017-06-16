#include "vpmu.hpp"                   // Include common C and C++ headers
#include "phase/phase.hpp"            // Phase class
#include "phase/phase-detect.hpp"     // PhaseDetect class
#include "phase/phase-classifier.hpp" // PhaseClassfier
#include "event-tracing.hpp"          // EventTracer
#include "vpmu-insn.hpp"              // InsnStream
#include "vpmu-cache.hpp"             // CacheStream
#include "vpmu-branch.hpp"            // BranchStream
#include "vpmu-snapshot.hpp"          // VPMUSanpshot
#include "vpmu-log.hpp"               // CONSOLE_LOG, VPMULog

// Put your own phase classifier below
#include "phase/phase-classifier-nn.hpp" // NearestCluster classifier

// Put you own phase classifier above

Phase Phase::not_found = Phase();

PhaseDetect phase_detect(DEFAULT_WINDOW_SIZE, std::make_unique<NearestCluster>());

void PhaseDetect::dump_data(FILE* fp, VPMU_Insn::Data data)
{
#define VPMU_INSN_SUM(_N) data.user._N + data.system._N
#define FILE_U64(str, val) fprintf(fp, str " %'" PRIu64 "\n", (uint64_t)val)
    FILE_U64(" Total instruction count       :", VPMU_INSN_SUM(total_insn));
    FILE_U64("  ->User mode insn count       :", data.user.total_insn);
    FILE_U64("  ->Supervisor mode insn count :", data.system.total_insn);
    FILE_U64("  ->deprecated                 :", 0);
    FILE_U64("  ->deprecated                 :", 0);
    FILE_U64(" Total load instruction count  :", VPMU_INSN_SUM(load));
    FILE_U64("  ->User mode load count       :", data.user.load);
    FILE_U64("  ->Supervisor mode load count :", data.system.load);
    FILE_U64("  ->deprecated                 :", 0);
    FILE_U64("  ->deprecated                 :", 0);
    FILE_U64(" Total store instruction count :", VPMU_INSN_SUM(store));
    FILE_U64("  ->User mode store count      :", data.user.store);
    FILE_U64("  ->Supervisor mode store count:", data.system.store);
    FILE_U64("  ->deprecated                 :", 0);
    FILE_U64("  ->deprecated                 :", 0);

#undef VPMU_INSN_SUM
#undef FILE_U64
}

void PhaseDetect::dump_data(FILE* fp, VPMU_Branch::Data data)
{
    int i;
    fprintf(fp, "    -> predict accuracy    : (");
    for (i = 0; i < VPMU.platform.cpu.cores - 1; i++) {
        fprintf(
          fp, "%'0.2f, ", (float)data.correct[i] / (data.correct[i] + data.wrong[i]));
    }
    fprintf(fp, "%'0.2f)\n", (float)data.correct[i] / (data.correct[i] + data.wrong[i]));
    // Correct
    fprintf(fp, "    -> correct prediction  : (");
    for (i = 0; i < VPMU.platform.cpu.cores - 1; i++) {
        fprintf(fp, "%'" PRIu64 ", ", data.correct[i]);
    }
    fprintf(fp, "%'" PRIu64 ")\n", data.correct[i]);
    // Wrong
    fprintf(fp, "    -> wrong prediction    : (");
    for (i = 0; i < VPMU.platform.cpu.cores - 1; i++) {
        fprintf(fp, "%'" PRIu64 ", ", data.wrong[i]);
    }
    fprintf(fp, "%'" PRIu64 ")\n", data.wrong[i]);
}

void PhaseDetect::dump_data(FILE* fp, VPMU_Cache::Data data)
{
    // Dump info
    fprintf(fp,
            "       (Miss Rate)   "
            "|    Access Count    "
            "|   Read Miss Count  "
            "|  Write Miss Count  "
            "|\n");
    // Memory
    fprintf(fp,
            "    -> memory (%0.2lf) |%'20" PRIu64 "|%'20" PRIu64 "|%'20" PRIu64 "|\n",
            (double)0.0,
            (uint64_t)data.memory_accesses,
            (uint64_t)0,
            (uint64_t)0);

    for (int j = 0; j < VPMU.platform.cpu.cores; j++) {
        // i-cache
        fprintf(fp,
                "    -> L%d-I   (%0.2lf) |%'20" PRIu64 "|%'20" PRIu64 "|%'20" PRIu64
                "|\n",
                1,
                (double)data.insn_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ_MISS]
                  / data.insn_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ],
                (uint64_t)data.insn_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ],
                (uint64_t)data.insn_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ_MISS],
                (uint64_t)0);
        uint64_t r_miss_count =
          data.data_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ_MISS];
        uint64_t w_miss_count =
          data.data_cache[PROCESSOR_CPU][1][j][VPMU_Cache::WRITE_MISS];
        uint64_t miss_count = r_miss_count + w_miss_count;

        uint64_t total_count = data.data_cache[PROCESSOR_CPU][1][j][VPMU_Cache::READ]
                               + data.data_cache[PROCESSOR_CPU][1][j][VPMU_Cache::WRITE];

        // d-cache
        fprintf(fp,
                "    -> L%d-D   (%0.2lf) |%'20" PRIu64 "|%'20" PRIu64 "|%'20" PRIu64
                "|\n",
                1,
                (double)miss_count / total_count,
                (uint64_t)total_count,
                (uint64_t)r_miss_count,
                (uint64_t)w_miss_count);
    }

    for (int i = VPMU_Cache::L2_CACHE; i < VPMU_Cache::L3_CACHE; i++) {
        for (int j = 0; j < VPMU.platform.cpu.cores; j++) {
            uint64_t r_miss_count =
              data.data_cache[PROCESSOR_CPU][i][j][VPMU_Cache::READ_MISS];
            uint64_t w_miss_count =
              data.data_cache[PROCESSOR_CPU][i][j][VPMU_Cache::WRITE_MISS];
            uint64_t miss_count = r_miss_count + w_miss_count;
            uint64_t total_count =
              data.data_cache[PROCESSOR_CPU][i][j][VPMU_Cache::READ]
              + data.data_cache[PROCESSOR_CPU][i][j][VPMU_Cache::WRITE];

            if (total_count == 0) continue;
            // d-cache
            fprintf(fp,
                    "    -> L%d-D   (%0.2lf) |%'20" PRIu64 "|%'20" PRIu64 "|%'20" PRIu64
                    "|\n",
                    i,
                    (double)miss_count / total_count,
                    (uint64_t)total_count,
                    (uint64_t)r_miss_count,
                    (uint64_t)w_miss_count);
        }
    }
}

void Phase::dump_result(FILE* fp)
{
#define FILE_TME(str, val) fprintf(fp, str " %'lf sec\n", (double)val / 1000000000.0)
    fprintf(fp, "==== Program Profile ====\n\n");
    fprintf(fp, "   === QEMU/ARM ===\n");
    fprintf(fp, "Instructions:\n");
    phase_detect.dump_data(fp, snapshot.insn_data);
    fprintf(fp, "Branch:\n");
    phase_detect.dump_data(fp, snapshot.branch_data);
    fprintf(fp, "CACHE:\n");
    phase_detect.dump_data(fp, snapshot.cache_data);
    fprintf(fp, "\n");
    fprintf(fp, "Timing Info:\n");
    FILE_TME("  ->CPU                        :", snapshot.time_ns[0]);
    FILE_TME("  ->Branch                     :", snapshot.time_ns[1]);
    FILE_TME("  ->Cache                      :", snapshot.time_ns[2]);
    FILE_TME("  ->System memory              :", snapshot.time_ns[3]);
    FILE_TME("  ->I/O memory                 :", snapshot.time_ns[4]);
    FILE_TME("Estimated execution time       :", snapshot.time_ns[5]);
#undef FILE_TME
}

// TODO Output better features for machine learning
void Phase::dump_metadata(FILE* fp)
{
    fprintf(fp, "0 ");
    fprintf(fp, "1:%lf ", (double)counters.alu / counters.insn);
    fprintf(fp, "2:%lf ", (double)counters.bit / counters.insn);
    fprintf(fp, "3:%lf ", (double)counters.load / counters.insn);
    fprintf(fp, "4:%lf ", (double)counters.store / counters.insn);
    fprintf(fp, "5:%lf ", (double)counters.branch / counters.insn);
    fprintf(fp, "\n\n");
}

void Phase::update_snapshot(VPMUSnapshot& process_snapshot)
{
    // TODO use async call back, the current counters are out of date
    {
        // If this is implemented in async mode, this force sync could be removed
        VPMU_sync();
        VPMUSnapshot new_snapshot(true);

        this->snapshot += new_snapshot - process_snapshot;
        process_snapshot = new_snapshot;
    }
}

inline void Window::update_vector(uint64_t pc)
{
    // Get the hased index for current pc address
    uint64_t hashed_key = vpmu::math::simple_hash(pc / 4, branch_vector.size());
    branch_vector[hashed_key]++;
}

inline void Window::update_counter(const ExtraTBInfo* extra_tb_info)
{
    counters.insn += extra_tb_info->counters.total;
    counters.load += extra_tb_info->counters.load;
    counters.store += extra_tb_info->counters.store;
    counters.alu += extra_tb_info->counters.alu;
    counters.bit += extra_tb_info->counters.bit;
    counters.branch += extra_tb_info->has_branch;
}

inline void Window::update(const ExtraTBInfo* extra_tb_info)
{
    uint64_t pc     = extra_tb_info->start_addr;
    uint64_t pc_end = pc + extra_tb_info->counters.size_bytes;
    // Update timestamp if this window is cleared before,
    // which means this is a new window.
    if (timestamp == 0) timestamp = vpmu_get_timestamp_us();
    update_vector(pc);
    update_counter(extra_tb_info);
    instruction_count += extra_tb_info->counters.total;

    auto&& key = std::make_pair(pc, pc_end);
    code_walk_count[key] += 1;
}

static void
update_phase(uint64_t pc, std::shared_ptr<ET_Process>& process, Window& window)
{
    // Classify the window to phase
    auto& phase = phase_detect.classify(process->phase_list, window);
    if (phase == Phase::not_found) {
        uint64_t phase_num = process->phase_list.size();
        // Add a new phase to the process
        DBG(
          STR_PHASE "pid: %" PRIu64 ", name: %s\n", process->pid, process->name.c_str());
        DBG(STR_PHASE "Create new phase id %zu @ PC: %p and Timestamp: %lu ms\n",
            phase_num,
            (void*)pc,
            window.timestamp / 1000);
        VPMU_sync();
        // Construct the instance inside vector (save time)
        process->phase_list.push_back(window);
        // We must update the snapshot to get the correct result
        auto&& new_phase = process->phase_list.back();
        new_phase.update_snapshot(process->snapshot);
        new_phase.id = phase_num;
        process->phase_history.push_back(std::make_pair(window.timestamp, phase_num));
    } else {
        uint64_t phase_num = (&phase - &process->phase_list[0]);
        // DBG(STR_PHASE "Update phase id %zu\n", phase_num);
        phase.update(window);
        phase.update_snapshot(process->snapshot);
        process->phase_history.push_back(std::make_pair(window.timestamp, phase_num));
    }
}

/*
static void
push_new_sub_phase(uint64_t pc, std::shared_ptr<ET_Process>& process, Window& window)
{
    uint64_t phase_num = process->phase_list.size();
    // Empty list, create a new phase with sub phase
    // Construct the instance inside vector (save time)
    process->phase_list.push_back(window);
    // We must update the snapshot to get the correct result
    auto&& new_phase = process->phase_list.back();
    new_phase.update_snapshot(process->snapshot);
    new_phase.id             = phase_num;
    new_phase.sub_phase_flag = true;
    // DBG(STR_PHASE "pid: %" PRIu64 ", name: %s\n", et_current_pid,
    // process->name.c_str());
    // DBG(STR_PHASE "Create new sub-phase @ PC: %p\n", (void*)pc);
}
*/

static void
update_window(uint64_t pid, const ExtraTBInfo* extra_tb_info, uint64_t stack_ptr)
{
#ifdef CONFIG_VPMU_DEBUG_MSG
    static uint64_t window_cnt = 0;
#endif
    uint64_t pc = extra_tb_info->start_addr;

    auto process = event_tracer.find_process(pid);
    if (process != nullptr) {
        // The process is being traced. Do phase detection.
        auto& current_window = process->current_window;
        // uint64_t last_sp        = process->stack_ptr;
        current_window.update(extra_tb_info);

        if (current_window.instruction_count > phase_detect.get_window_size()) {
#ifdef CONFIG_VPMU_DEBUG_MSG
            window_cnt++;
            if (window_cnt % 100 == 0)
                DBG(STR_PHASE "Timestamp (# windows): %'9" PRIu64 "\n", window_cnt);
#endif
            auto& last_phase = process->phase_list.back();
            if (process->phase_list.size() != 0 && last_phase.sub_phase_flag == true) {
                last_phase.sub_phase_flag = false;
            }
            update_phase(pc, process, current_window);
            // Reset all counters and vars of current window
            current_window.reset();
        }
        /*
        else {
            if (stack_ptr < last_sp) {
                auto& last_phase = process->phase_list.back();
                if (process->phase_list.size() == 0
                    || last_phase.sub_phase_flag == false) {
                    // Create a new sub-phase
                    push_new_sub_phase(pc, process, current_window);
                } else {
                    last_phase.update(current_window);
                    if (last_phase.get_insn_count() > phase_detect.get_window_size()) {
#ifdef CONFIG_VPMU_DEBUG_MSG
                        window_cnt++;
                        if (window_cnt % 100 == 0)
                            DBG(STR_PHASE "Timestamp (# windows): %'9" PRIu64 "\n",
                                window_cnt);
#endif
                        last_phase.update_snapshot(process->snapshot);
                        // Promote the last sub phase to a regular phase
                        last_phase.sub_phase_flag = false;
                        auto& phase =
                          phase_detect.classify(process->phase_list, last_phase);
                        if (phase == Phase::not_found) {
                            DBG(STR_PHASE "pid: %" PRIu64 ", name: %s\n",
                                process->pid,
                                process->name.c_str());
                            DBG(
                              STR_PHASE
                              "Create new phase id %zu @ PC: %p and Timestamp: %lu ms\n",
                              last_phase.id,
                              (void*)pc,
                              current_window.timestamp / 1000);
                            process->phase_history.push_back(
                              std::make_pair(current_window.timestamp, last_phase.id));
                        } else {
                            // Merge two phases
                            phase.update(last_phase);
                            process->phase_list.pop_back();
                            process->phase_history.push_back(
                              std::make_pair(current_window.timestamp, phase.id));
                        }
                    }
                }
                // Reset all counters and vars of current window
                current_window.reset();
            }
        }
    */

        process->stack_ptr = stack_ptr;
    }
}

void phasedet_ref(bool               user_mode,
                  const ExtraTBInfo* extra_tb_info,
                  uint64_t           stack_ptr,
                  uint64_t           core_id)
{
    // TODO This won't apply to multi-core emulation
    static bool last_tb_is_user = false;
    if (!user_mode) {
        // kernel_phasedet_ref((last_tb_is_user == true), extra_tb_info);
        last_tb_is_user = false;
    } else {
        uint64_t pid = VPMU.core[vpmu::get_core_id()].current_pid;
        if (last_tb_is_user == false) {
            // Kernel IRQ to User mode
            // auto process = event_tracer.find_process((uint64_t)0);
            // if (process != nullptr)
            //    process->phase_history.push_back(std::make_pair(vpmu_get_timestamp_us(),
            //    -1));
        }
        last_tb_is_user = user_mode;
        update_window(pid, extra_tb_info, stack_ptr);
    }

    return;
}
