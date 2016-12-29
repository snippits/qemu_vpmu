#ifndef __VPMU_CACHE_HPP_
#define __VPMU_CACHE_HPP_
extern "C" {
#include "vpmu-cache.h"
}
#include "vpmu.hpp"        // VPMU common header
#include "vpmu-stream.hpp" // VPMUStream, VPMUStream_T
#include "vpmu-packet.hpp" // VPMU_Inst, VPMU_Branch, VPMU_Cache
#include "json.hpp"        // nlohmann::json
// The implementaion of stream buffer and multi- threading/processing
#include "stream/single-thread.hpp" // VPMU_Stream_Single_Thread
#include "stream/multi-thread.hpp"  // VPMUStreamMultiThread
#include "stream/multi-process.hpp" // VPMU_Stream_Multi_Process

class CacheStream : public VPMUStream_T<VPMU_Cache>
{
public:
    CacheStream() : VPMUStream_T<VPMU_Cache>("CACHE") { log_debug("Constructed"); }
    CacheStream(const char* module_name) : VPMUStream_T<VPMU_Cache>(module_name) {}
    CacheStream(std::string module_name) : VPMUStream_T<VPMU_Cache>(module_name) {}

    void set_default_stream_impl(void) override
    {
        // Get the default implementation of stream interface.
        impl = std::make_unique<VPMUStreamMultiProcess<VPMU_Cache>>("C_Strm");
        // impl = std::make_unique<VPMUStreamMultiThread<VPMU_Cache>>("C_Strm");
        // impl = std::make_unique<VPMUStreamSingleThread<VPMU_Cache>>("C_Strm");
        // Construct the channel (buffer) and allocate resources
        impl->build(1024 * 64);
    }

    void send(uint8_t proc, uint8_t core, uint64_t addr, uint16_t type, uint16_t size);
    void
    send_hot_tb(uint8_t proc, uint8_t core, uint64_t addr, uint16_t type, uint16_t size);

    inline uint64_t get_cache_cycles(int n)
    {
        VPMU_Cache::Model model  = get_model(n);
        VPMU_Cache::Data  data   = get_data(n);
        uint64_t          cycles = 0;

        { // VPMU_Cache::L1_CACHE
            int level = VPMU_Cache::L1_CACHE;
            // TODO multi-core, how to calculate timing
            int      core     = 0;
            uint64_t miss_cnt = 0, hit_cnt = 0;
            { // Instruction cache
                auto& cache = data.inst_cache[PROCESSOR_CPU][level][core];
                miss_cnt += cache[VPMU_Cache::READ_MISS];
                hit_cnt += cache[VPMU_Cache::READ] - cache[VPMU_Cache::READ_MISS];
            }

            { // Data cache
                auto& cache = data.data_cache[PROCESSOR_CPU][level][core];
                miss_cnt += cache[VPMU_Cache::READ_MISS] + cache[VPMU_Cache::WRITE_MISS];
                hit_cnt += cache[VPMU_Cache::READ] + cache[VPMU_Cache::WRITE]
                           - cache[VPMU_Cache::READ_MISS] - cache[VPMU_Cache::WRITE_MISS];
            }
            cycles += model.latency[level] * miss_cnt + 1 * hit_cnt;
        }

        for (int level = VPMU_Cache::L2_CACHE; level <= model.levels; level++) {
            uint64_t miss_cnt = 0, hit_cnt = 0;
            for (int core = 0; core < VPMU.platform.cpu.cores; core++) {
                // Data cache
                auto& cache = data.data_cache[PROCESSOR_CPU][level][core];
                miss_cnt += cache[VPMU_Cache::READ_MISS] + cache[VPMU_Cache::WRITE_MISS];
                hit_cnt += cache[VPMU_Cache::READ] + cache[VPMU_Cache::WRITE]
                           - cache[VPMU_Cache::READ_MISS] - cache[VPMU_Cache::WRITE_MISS];
            }
            cycles += model.latency[level] * miss_cnt + 1 * hit_cnt;
        }

        return cycles;
    }

    inline uint64_t get_memory_time_ns(int n)
    {
        VPMU_Cache::Data data = get_data(n);
        return data.memory_time_ns;
    }

    inline uint64_t get_cycles(int n)
    {
        return get_memory_time_ns(n) / vpmu::target::scale_factor() + get_cache_cycles(n);
    }

    inline uint64_t get_cycles(void) { return get_cycles(0); }

private:
    // This is a register function declared in the vpmu-cache.cc file.
    Sim_ptr create_sim(std::string sim_name) override;
    // This is for JIT-model selection
    bool data_possibly_hit(uint64_t addr, uint32_t rw);
};

extern CacheStream vpmu_cache_stream;
#endif
