#include "vpmu-template-output.hpp"
#include "vpmu-insn.hpp"   // InsnStream
#include "vpmu-cache.hpp"  // CacheStream
#include "vpmu-branch.hpp" // BranchStream

// We use 17 digits plus 3 characters (20 in total) to ensure a
// pretty output on the minimum 80 characters (width) tty console.
#define U64_20C "%'17" PRIu64 " | "
#define F64_20C "%'17.2lf | "

namespace vpmu
{

namespace output
{
    void u64_array(uint64_t value[])
    {
        vpmu::dump::u64_array(vpmu_console_log_fd, value);
    }

    void percentage_array(uint64_t first_val[], uint64_t second_val[])
    {
        vpmu::dump::percentage_array(vpmu_console_log_fd, first_val, second_val);
    }

    void CPU_counters(VPMU_Insn::Model model, VPMU_Insn::Data data)
    {
        vpmu::dump::CPU_counters(vpmu_console_log_fd, model, data);
    }

    void Branch_counters(VPMU_Branch::Model model, VPMU_Branch::Data data)
    {
        vpmu::dump::Branch_counters(vpmu_console_log_fd, model, data);
    }

    void Cache_counters(VPMU_Cache::Model model, VPMU_Cache::Data data)
    {
        vpmu::dump::Cache_counters(vpmu_console_log_fd, model, data);
    }

} // End of namespace vpmu::output

namespace dump
{
    void u64_array(FILE* fp, uint64_t value[])
    {
        int i = 0;
        fprintf(fp, " ");
        // Fold when tty is too narrow in tty output
        if ((fp == stderr || fp == stdout)
            && (VPMU.platform.cpu.cores > 4
                || vpmu::utils::get_tty_columns() < 20 * VPMU.platform.cpu.cores + 36)) {
            fprintf(fp, "\n");
        }
        for (i = 0; i < VPMU.platform.cpu.cores; i++) {
            fprintf(fp, U64_20C, value[i]);
            if (i != 0 && i % 4 == 0) fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
    }

    void percentage_array(FILE* fp, uint64_t first_val[], uint64_t second_val[])
    {
        int i = 0;
        fprintf(fp, " ");
        // Fold when tty is too narrow in tty output
        if ((fp == stderr || fp == stdout)
            && (VPMU.platform.cpu.cores > 4
                || vpmu::utils::get_tty_columns() < 20 * VPMU.platform.cpu.cores + 36)) {
            fprintf(fp, "\n");
        }
        for (i = 0; i < VPMU.platform.cpu.cores; i++) {
            double perc = (double)first_val[i] / (first_val[i] + second_val[i] + 1);
            fprintf(fp, F64_20C, perc);
            if (i != 0 && i % 4 == 0) fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
    }

    void CPU_counters(FILE* fp, VPMU_Insn::Model model, VPMU_Insn::Data data)
    {
        using vpmu::math::sum_cores;

        FILE_FP_U64(fp, " Total cycle count              :", data.sum_all().cycles);
        FILE_FP_U64(fp, " Total instruction count        :", data.sum_all().total_insn);
        fprintf(fp, "   ->User mode insn count       :");
        u64_array(fp, data.user.total_insn);
        fprintf(fp, "   ->Supervisor mode insn count :");
        u64_array(fp, data.system.total_insn);
        FILE_FP_U64(fp, " Total load instruction count   :", data.sum_all().load);
        fprintf(fp, "   ->User mode load count       :");
        u64_array(fp, data.user.load);
        fprintf(fp, "   ->Supervisor mode load count :");
        u64_array(fp, data.system.load);
        FILE_FP_U64(fp, " Total store instruction count  :", data.sum_all().store);
        fprintf(fp, "   ->User mode store count      :");
        u64_array(fp, data.user.store);
        fprintf(fp, "   ->Supervisor mode store count:");
        u64_array(fp, data.system.store);
    }

    void Branch_counters(FILE* fp, VPMU_Branch::Model model, VPMU_Branch::Data data)
    {
        using vpmu::math::sum_cores;

        // Accuracy
        fprintf(fp, "    -> predict accuracy         :");
        percentage_array(fp, data.correct, data.wrong);
        // Correct
        fprintf(fp, "    -> correct prediction       :");
        u64_array(fp, data.correct);
        // Wrong
        fprintf(fp, "    -> wrong prediction         :");
        u64_array(fp, data.wrong);
    }

    void Cache_counters(FILE* fp, VPMU_Cache::Model model, VPMU_Cache::Data data)
    {
        // Dump info
        fprintf(fp,
                "       (Miss Rate)     "
                "|    Access Count   "
                "|  Read Miss Count  "
                "|  Write Miss Count "
                "|\n");

        fprintf(fp,
                "    -> memory   (%0.2lf) | " U64_20C U64_20C U64_20C "\n",
                (double)0.0,
                (uint64_t)data.memory_accesses,
                (uint64_t)0,
                (uint64_t)0);

        for (int l = model.levels; l >= VPMU_Cache::L2_CACHE; l--) {
            auto&&   c       = data.data_cache[PROCESSOR_CPU][l][0];
            uint64_t rw      = c[VPMU_Cache::READ] + c[VPMU_Cache::WRITE];
            uint64_t rw_miss = c[VPMU_Cache::READ_MISS] + c[VPMU_Cache::WRITE_MISS];

            // Index start from the last level of cache to the L2 cache
            fprintf(fp,
                    "    -> L%d-D     (%0.2lf) | " U64_20C U64_20C U64_20C "\n",
                    l,
                    (double)rw_miss / (rw + 1),
                    (uint64_t)(rw),
                    (uint64_t)(c[VPMU_Cache::READ_MISS]),
                    (uint64_t)(c[VPMU_Cache::WRITE_MISS]));
        }

        for (int i = 0; i < VPMU.platform.cpu.cores; i++) {
            auto&&   c       = data.data_cache[PROCESSOR_CPU][VPMU_Cache::L1_CACHE][i];
            uint64_t rw      = c[VPMU_Cache::READ] + c[VPMU_Cache::WRITE];
            uint64_t rw_miss = c[VPMU_Cache::READ_MISS] + c[VPMU_Cache::WRITE_MISS];

            fprintf(fp,
                    "    -> L1-D[%2d] (%0.2lf) | " U64_20C U64_20C U64_20C "\n",
                    i,
                    (double)rw_miss / (rw + 1),
                    (uint64_t)(rw),
                    (uint64_t)(c[VPMU_Cache::READ_MISS]),
                    (uint64_t)(c[VPMU_Cache::WRITE_MISS]));

            auto&&   ci       = data.insn_cache[PROCESSOR_CPU][VPMU_Cache::L1_CACHE][i];
            uint64_t irw      = ci[VPMU_Cache::READ] + ci[VPMU_Cache::WRITE];
            uint64_t irw_miss = ci[VPMU_Cache::READ_MISS] + ci[VPMU_Cache::WRITE_MISS];

            fprintf(fp,
                    "    -> L1-I[%2d] (%0.2lf) | " U64_20C U64_20C U64_20C "\n",
                    i,
                    (double)irw_miss / (irw + 1),
                    (uint64_t)(irw),
                    (uint64_t)(ci[VPMU_Cache::READ_MISS]),
                    (uint64_t)(ci[VPMU_Cache::WRITE_MISS]));
        }
    }

    void snapshot(FILE* fp, VPMUSnapshot snapshot)
    {
#define FILE_TME(str, val) fprintf(fp, str " %'lf sec\n", (double)val / 1000000000.0)
        fprintf(fp, "==== Program Profile ====\n\n");
        fprintf(fp, "   === QEMU/ARM ===\n");
        fprintf(fp, "Instructions:\n");
        CPU_counters(fp, vpmu_insn_stream.get_model(), snapshot.insn_data);
        fprintf(fp, "Branch:\n");
        Branch_counters(fp, vpmu_branch_stream.get_model(), snapshot.branch_data);
        fprintf(fp, "CACHE:\n");
        Cache_counters(fp, vpmu_cache_stream.get_model(), snapshot.cache_data);
        fprintf(fp, "\n");
        fprintf(fp, "Timing Info:\n");
        FILE_TME("  ->CPU                        :", snapshot.time_ns[0]);
        FILE_TME("  ->Branch                     :", snapshot.time_ns[1]);
        FILE_TME("  ->Cache                      :", snapshot.time_ns[2]);
        FILE_TME("  ->System memory              :", snapshot.time_ns[3]);
        FILE_TME("  ->I/O memory                 :", snapshot.time_ns[4]);
        FILE_TME("Estimated execution time       :", snapshot.time_ns[5]);
        FILE_TME("Host emulation time            :", snapshot.time_ns[6]);
#undef FILE_TME
    }

} // End of namespace vpmu::dump

namespace dump_json
{
    void CPU_counters(nlohmann::json& j, VPMU_Insn::Model model, VPMU_Insn::Data data)
    {
        using vpmu::math::sum_cores;

        // Reduce counter values across cores to the first element
        data.reduce();

        j["instruction"]["cycle"]  = data.sum_all().cycles;
        j["instruction"]["total"]  = data.sum_all().total_insn;
        j["instruction"]["user"]   = data.user.total_insn[0];
        j["instruction"]["system"] = data.system.total_insn[0];

        j["load"]["total"]  = data.sum_all().load;
        j["load"]["user"]   = data.user.load[0];
        j["load"]["system"] = data.system.load[0];

        j["store"]["total"]  = data.sum_all().store;
        j["store"]["user"]   = data.user.store[0];
        j["store"]["system"] = data.system.store[0];
    }

    void
    Branch_counters(nlohmann::json& j, VPMU_Branch::Model model, VPMU_Branch::Data data)
    {
        using vpmu::math::sum_cores;

        // Reduce counter values across cores to the first element
        data.reduce();
        double accuracy = (double)data.correct[0] / (data.correct[0] + data.wrong[0]);

        j["branch"]["accuracy"] = accuracy;
        j["branch"]["hit"]      = data.correct[0];
        j["branch"]["miss"]     = data.wrong[0];
    }

    void Cache_counters(nlohmann::json& j, VPMU_Cache::Model model, VPMU_Cache::Data data)
    {
        // Reduce counter values across cores to the first element
        data.reduce();

        for (int l = model.levels; l >= VPMU_Cache::L2_CACHE; l--) {
            auto&&   c       = data.data_cache[PROCESSOR_CPU][l][0];
            uint64_t rw      = c[VPMU_Cache::READ] + c[VPMU_Cache::WRITE];
            uint64_t rw_miss = c[VPMU_Cache::READ_MISS] + c[VPMU_Cache::WRITE_MISS];

            std::string level_str = "level" + std::to_string(l);

            j["cache"][level_str]["missRate"]    = (double)rw_miss / (rw + 1);
            j["cache"][level_str]["accessCount"] = rw;
            j["cache"][level_str]["readMiss"]    = c[VPMU_Cache::READ_MISS];
            j["cache"][level_str]["writeMiss"]   = c[VPMU_Cache::WRITE_MISS];
        }

        auto&&   cd      = data.data_cache[PROCESSOR_CPU][VPMU_Cache::L1_CACHE][0];
        uint64_t rw      = cd[VPMU_Cache::READ] + cd[VPMU_Cache::WRITE];
        uint64_t rw_miss = cd[VPMU_Cache::READ_MISS] + cd[VPMU_Cache::WRITE_MISS];

        auto&&   ci       = data.insn_cache[PROCESSOR_CPU][VPMU_Cache::L1_CACHE][0];
        uint64_t irw      = ci[VPMU_Cache::READ] + ci[VPMU_Cache::WRITE];
        uint64_t irw_miss = ci[VPMU_Cache::READ_MISS] + ci[VPMU_Cache::WRITE_MISS];

        j["cache"]["dCache"]["missRate"]    = (double)rw_miss / (rw + 1);
        j["cache"]["dCache"]["accessCount"] = rw;
        j["cache"]["dCache"]["readMiss"]    = cd[VPMU_Cache::READ_MISS];
        j["cache"]["dCache"]["writeMiss"]   = cd[VPMU_Cache::WRITE_MISS];

        j["cache"]["iCache"]["missRate"]    = (double)irw_miss / (irw + 1);
        j["cache"]["iCache"]["accessCount"] = irw;
        j["cache"]["iCache"]["readMiss"]    = ci[VPMU_Cache::READ_MISS];
        j["cache"]["iCache"]["writeMiss"]   = ci[VPMU_Cache::WRITE_MISS];
    }

    nlohmann::json snapshot(VPMUSnapshot snapshot)
    {
        nlohmann::json j;

        CPU_counters(j, vpmu_insn_stream.get_model(), snapshot.insn_data);
        Branch_counters(j, vpmu_branch_stream.get_model(), snapshot.branch_data);
        Cache_counters(j, vpmu_cache_stream.get_model(), snapshot.cache_data);

        j["time"]["cpu"]          = snapshot.time_ns[0];
        j["time"]["branch"]       = snapshot.time_ns[1];
        j["time"]["cache"]        = snapshot.time_ns[2];
        j["time"]["systemMemory"] = snapshot.time_ns[3];
        j["time"]["ioMemory"]     = snapshot.time_ns[4];
        j["time"]["target"]       = snapshot.time_ns[5];
        j["time"]["host"]         = snapshot.time_ns[6];

        return j;
    }

} // End of namespace vpmu::dump_json
} // End of namespace vpmu
