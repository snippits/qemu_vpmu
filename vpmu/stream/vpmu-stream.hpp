#ifndef __VPMU_STREAM_HPP_
#define __VPMU_STREAM_HPP_
#pragma once

extern "C" {
#include "vpmu-qemu.h" // VPMUPlatformInfo
}
#include <mutex>                 // Mutex
#include "vpmu-local-buffer.hpp" // VPMULocalBuffer
#include "vpmu-sim.hpp"          // VPMUSimulator
#include "vpmu-stream-impl.hpp"  // VPMUStream_Impl
#include "json.hpp"              // nlohmann::json

class VPMUStream : public VPMULog
{
public:
    VPMUStream() {}
    virtual ~VPMUStream() {}
    VPMUStream(const char* module_name) : VPMULog(module_name) {}
    VPMUStream(std::string module_name) : VPMULog(module_name) {}
    // VPMUStream is neither copyable nor movable.
    VPMUStream(const VPMUStream&) = delete;
    VPMUStream& operator=(const VPMUStream&) = delete;

    virtual void set_default_stream_impl(void)
    {
        LOG_FATAL("set_default_stream_impl is not implemented");
    }
    virtual void bind(nlohmann::json) { LOG_FATAL("bind is not implemented"); }
    virtual bool build(void)
    {
        LOG_FATAL("build is not implemented");
        return false;
    }
    virtual void destroy(void) { LOG_FATAL("destroy is not implemented"); }
    virtual void reset(void) { LOG_FATAL("reset is not implemented"); }
    virtual void sync(void) { LOG_FATAL("sync is not implemented"); }
    virtual void sync_none_blocking(void)
    {
        LOG_FATAL("sync_none_blocking is not implemented");
    }
    virtual void dump(void) { LOG_FATAL("dump is not implemented"); }
};

template <typename T>
class VPMUStream_T : public VPMUStream
{
public:
    // Aliasing the real implementation this stream is using
    using Sim_ptr     = std::unique_ptr<VPMUSimulator<T>>;
    using Impl_ptr    = std::unique_ptr<VPMUStream_Impl<T>>;
    using TraceBuffer = typename T::TraceBuffer;
    using Reference   = typename T::Reference;
    using Model       = typename T::Model;
    using Data        = typename T::Data;

    VPMUStream_T() {}
    VPMUStream_T(const char* module_name) : VPMUStream(module_name) {}
    VPMUStream_T(std::string module_name) : VPMUStream(module_name) {}
    virtual ~VPMUStream_T()
    {
        destroy();
        log_debug("Destructed");
    }
    // VPMUStream_T is neither copyable nor movable.
    VPMUStream_T(const VPMUStream_T&) = delete;
    VPMUStream_T& operator=(const VPMUStream_T&) = delete;

    void bind(nlohmann::json configs) override
    {
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        // std::cout << configs.dump();
        if (configs.size() < 1) {
            LOG_FATAL("There is no content!");
        }
        target_configs = configs;
    }

    bool build(void) override
    {
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        log_debug("Initializing");
        // Destroy worker jobs from last build
        jobs.clear(); // Clear arrays and call destructors
        for (auto& b : local_buffer) b.reset();

        // Get the default implementation of stream interface.
        if (impl == nullptr) {
            // Call child default implementation builder
            this->set_default_stream_impl();
        }
        if (impl->get_trace_buffer() == nullptr) {
            // Call build if buffer is not built yet.
            impl->build();
        }

        // Locate and create instances of simulator according to the name.
        if (target_configs.is_array()) {
            for (auto sim_config : target_configs) {
                attach_simulator(sim_config);
            }
        } else {
            attach_simulator(target_configs);
        }

        log_debug("Binding total %d simulators to stream and build them.", jobs.size());
        if (jobs.size() == 0) {
            log_fatal("# of total timing models cannot be zero!!");
            return false;
        }
        // Start worker threads/processes with its ring buffer implementation
        impl->run(jobs);

        log_debug("Initialized");
        return true;
    }

    void destroy(void) override
    {
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        // Only release resources here.
        // Do not clear states, ex: target_configs
        impl.reset(nullptr);
        // Call de-allocation of each simulator manually
        jobs.clear(); // Clear arrays and call destructors
        for (auto& b : local_buffer) b.reset();
    }

    void reset(void) override
    {
        // Basic safety check
        if (impl == nullptr) return;
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        clean_out_local_buff();
        impl->send_reset();
    }

    void sync(void) override
    {
        // Basic safety check
        if (impl == nullptr) return;
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        clean_out_local_buff();
        impl->send_sync();
    }

    void dump(void) override
    {
        // Basic safety check
        if (impl == nullptr) return;
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        clean_out_local_buff();
        impl->send_dump();
    }

    void sync_none_blocking(void) override
    {
        // Basic safety check
        if (impl == nullptr) return;
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_mutex);
        // log_debug("sync none blocking");
        clean_out_local_buff();
        impl->send_sync_none_blocking();
    }

    // Below are non-virtual public functions
    inline void send_ref(int core, Reference& new_ref)
    {
        // Basic safety check
        if (impl == nullptr) return;

        local_buffer[core].push_back(new_ref);
        if (unlikely(local_buffer[core].isFull())) {
            // lock is automatically released when lock goes out of scope
            std::lock_guard<std::mutex> lock(stream_mutex);
            impl->send(local_buffer[core].get_buffer(),
                       local_buffer[core].get_index(),
                       local_buffer[core].get_size());
            local_buffer[core].reset();
        }
    }

    void attach_simulator(nlohmann::json sim_config)
    {
        std::string sim_name = sim_config["name"];
        // lock is automatically released when lock goes out of scope
        std::lock_guard<std::mutex> lock(stream_simulator_mutex);

        log("Attaching... " BASH_COLOR_CYAN "%s" BASH_COLOR_NONE, sim_name.c_str());

        auto ptr = create_sim(sim_name);
        if (ptr == nullptr) {
            log(BASH_COLOR_RED "    not found" BASH_COLOR_NONE);
            return;
        }
        ptr->bind(sim_config);
        jobs.push_back(std::move(ptr));
    }

    void set_stream_impl(Impl_ptr&& s) { impl = std::move(s); }

    void set_default_stream_impl(void) override
    {
        LOG_FATAL("set_stream_impl is not implemented");
    }

    // Getter functions for C side to use.
    // Must use the inline hint to ensure the performance.
    inline Model        get_model(void) { return impl->get_model(0); }
    inline Data         get_data(void) { return impl->get_data(0); }
    inline TraceBuffer* get_ringbuffer(void) { return impl->get_trace_buffer(); }
    inline sem_t*       get_semaphore(void) { return impl->get_semaphore(0); }
    inline uint32_t     get_num_workers(void) { return impl->get_num_workers(); }

    // Overloading of get model function
    inline Model get_model(int n) { return impl->get_model(n); }
    // Overloading of get data function
    inline Data get_data(int n) { return impl->get_data(n); }

protected:
    // Force to clean out local buffer whenever the packet is a control packet
    // mutex lock and nullptr check should be done before this function
    // This function does not check any of them for performance
    inline void clean_out_local_buff(void)
    {
        // Flush out local buffer when index is not zero
        for (auto&& buf : local_buffer) {
            if (buf.isEmpty() == false) {
                impl->send(buf.get_buffer(), buf.get_index(), buf.get_size());
                buf.reset();
            }
        }
    }

    // The array of jobs (timing simulators)
    std::vector<Sim_ptr> jobs;
    // Impl
    Impl_ptr impl;

private:
    VPMULocalBuffer<Reference, 256> local_buffer[VPMU_MAX_CPU_CORES + VPMU_MAX_GPU_CORES];
    // A copy of configuration sent to simulators
    nlohmann::json target_configs;
    // This mutex protects: impl function calls, and stream interface functions
    std::mutex stream_mutex;
    // This mutex protects: creation of simulators
    std::mutex stream_simulator_mutex;

    virtual Sim_ptr create_sim(std::string sim_name)
    {
        LOG_FATAL("create_sim is not defined");
        return nullptr;
    };
};

#endif
