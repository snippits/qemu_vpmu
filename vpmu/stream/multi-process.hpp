#ifndef __VPMU_STREAM_MULTI_PROCESS_HPP_
#define __VPMU_STREAM_MULTI_PROCESS_HPP_
extern "C" {
#include <sys/types.h> // Types of kernel related (pid_t, etc.)
#include <signal.h>    // kill()
}
#include <thread>               // std::thread
#include <memory>               // Smart pointers and mem management
#include "vpmu-stream-impl.hpp" // VPMUStream_Impl
// Boost Library for inter-process communications
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

template <typename T>
class VPMUStreamMultiProcess : public VPMUStream_Impl<T>
{
public:
    using Reference   = typename T::Reference;
    using TraceBuffer = typename T::TraceBuffer;
    using Sim_ptr     = std::unique_ptr<VPMUSimulator<T>>;

public:
    VPMUStreamMultiProcess(std::string name, uint64_t num_elems)
        : VPMUStream_Impl<T>(name)
    {
        num_trace_buffer_elems = num_elems;
    }

    ~VPMUStreamMultiProcess() { destroy(); }

    void build() override
    {
        using namespace boost::interprocess;
        int total_buffer_size = Stream_Layout<T>::total_size(num_trace_buffer_elems);

        if (buffer != nullptr) {
            // shmdt(buffer);
            buffer = nullptr;
        }

        // TODO Need a hased name for safely initializing (forking) simulators when
        // concurrent VM execution is enabled. If hased named is implemented,
        // the deletion of shared memory needs to be guaranteed when crushing.
        // Erases objects from the system. Returns false on error. Never throws
        shared_memory_object::remove("vpmu_cache_ring_buffer");
        shm = shared_memory_object(create_only, "vpmu_cache_ring_buffer", read_write);

        // Set size
        shm.truncate(total_buffer_size);

        // Map the whole shared memory in this process
        region = mapped_region(shm, read_write);
        log_debug("Mapped address %p, size %d.", region.get_address(), region.get_size());
        // Write all the memory to 0
        std::memset(region.get_address(), 0, region.get_size());
        buffer = (uint8_t *)region.get_address();

        // Copy (by value) the CPU information to ring buffer
        platform_info  = Stream_Layout<T>::get_platform_info(buffer);
        *platform_info = VPMU.platform;
        // Assign pointer of common data
        stream_comm = Stream_Layout<T>::get_stream_comm(buffer);
        // Assign the pointer of token
        token = Stream_Layout<T>::get_token(buffer);
        // Assign the pointer of heart beat signal
        heart_beat = Stream_Layout<T>::get_heart_beat(buffer);
        // Assign pointer of trace buffer
        shm_bufferInit(trace_buffer,           // the pointer of trace buffer
                       num_trace_buffer_elems, // the number of packets
                       Reference,              // the type of packet
                       TraceBuffer,            // the type of trace buffer
                       Stream_Layout<T>::get_trace_buffer(buffer));

        // Initialize semaphores to zero, and set to process-shared!!
        for (int i = 0; i < VPMU_MAX_NUM_WORKERS; i++)
            sem_init(&stream_comm[i].job_semaphore, true, 0);
        log_debug("Common resource allocated");
    }

    void destroy(void) override
    {
        // De-allocating the thread resources appropriately, required by C++ standard
        if (heart_beat_thread.native_handle() != 0) {
            pthread_cancel(heart_beat_thread.native_handle());
            // Standard thread library require this for correct destructor behavior
            heart_beat_thread.join();
        }

        // De-allocating resources must be the opposite order of resource allocation
        for (auto &s : slaves) {
            kill(s, SIGKILL);
        }
        slaves.clear(); // Clear vector data, and call destructor automatically

        // Erases objects from the system. Returns false on error. Never throws
        boost::interprocess::shared_memory_object::remove("vpmu_cache_ring_buffer");
        if (buffer != nullptr) {
            // shmdt(buffer);
            buffer       = nullptr;
            trace_buffer = nullptr;
        }
    }

    void run(std::vector<Sim_ptr> &works) override
    {
        num_workers = works.size();
        // Initialize the data vector for synchronizing counters from worker threads

        for (int id = 0; id < works.size(); id++) {
            pid_t pid = fork();

            if (pid) {
                // Parent
                slaves.push_back(pid);
            } else {
                // This "move" improves the performance a little bit.
                // It doesn't affect the host process. :D
                auto sim = std::move(works[id]);
                // auto &sim = works[id];
                // Local buffers, the size is not necessary to be the same as sender
                const uint32_t local_buffer_size = 1024;
                Reference      local_buffer[local_buffer_size];
                int            num_refs = 0;

                vpmu::utils::name_process(this->get_name() + std::to_string(id));
                stream_comm = Stream_Layout<T>::get_stream_comm(buffer);
                // Initialize (build) the target simulation with its configuration
                sim->set_platform_info(*platform_info);
                sim->build(stream_comm[id].model);
                // Initialize mutex to one, and set to process-shared
                sem_init(&stream_comm[id].job_semaphore, true, 0);
                // Set synced_flag to tell master it's done
                stream_comm[id].synced_flag = true;
                log_debug("worker process %d start", id);
                while (1) {
                    this->wait_semaphore(id); // Down semaphore
                    // Keep draining traces till it's empty
                    while (likely(shm_isBufferNotEmpty(trace_buffer, id))) {
                        shm_bulkRead(
                          trace_buffer,      // Pointer to ringbuffer
                          id,                // ID of the worker
                          local_buffer,      // Pointer to local(private) buffer
                          local_buffer_size, //#elements of local(private) buffer
                          sizeof(Reference), // Size of each elements
                          num_refs);         //#elements read successfully

                        // Do simulation
                        for (int i = 0; i < num_refs; i++) {
                            if (unlikely(local_buffer[i].type == VPMU_PACKET_DUMP_INFO)) {
                                this->wait_token(id);
                                sim->packet_processor(
                                  id, local_buffer[i], stream_comm[id].data);
                                this->pass_token(id);
                            } else {
                                if (local_buffer[i].type & VPMU_PACKET_HOT) {
                                    sim->hot_packet_processor(
                                      id, local_buffer[i], stream_comm[id].data);
                                } else {
                                    sim->packet_processor(
                                      id, local_buffer[i], stream_comm[id].data);
                                }
                            }
                        }
                    }
                }
                // It should never return!!!
                abort();
            }
        }
        fork_zombie_killer();

        // Wait all forked process to be initialized
        if (this->timed_wait_sync_flag(5000) == false) {
            LOG_FATAL("Some component timing simulators might not be alive!");
            // some simulators are not responding
            // You know, I'm not always going to be around to help you - Charlie Brown
            ERR_MSG(
              "some forked simulators or remote process are not responding. \n"
              "\tThis might because the some zombie cache simulator exists.\n"
              "\tOr custom cache simulator were not executed after qemu's execution\n"
              "\tTry \"killall qemu-system-arm\" to solve zombie processes.\n");
            exit(EXIT_FAILURE);
        }
    }

    void send(Reference *local_buffer, uint32_t num_refs, uint32_t total_size) override
    {
        // Basic safety check
        if (trace_buffer == nullptr) return;

#ifdef CONFIG_VPMU_DEBUG_MSG
        debug_packet_num_cnt += num_refs;
#endif

        // Periodically sync back counters for timing
        static uint32_t cnt = 0;
        cnt++;
        if (cnt == 4) {
            Reference barrier;

            barrier.type = VPMU_PACKET_BARRIER;
            send(barrier);
            cnt = 0;
        }

        for (int i = 0; i < num_workers; i++)
            while (shm_remainedBufferSpace(trace_buffer, i) <= total_size) usleep(1);
        shm_bulkWrite(trace_buffer,       // Pointer to ringbuffer
                      local_buffer,       // Pointer to local(private) buffer
                      num_refs,           // Number of elements to write
                      sizeof(Reference)); // Size of each elements
        this->post_semaphore();           // up semaphores
    }

    void send(Reference &ref) override
    {
        // Basic safety check
        if (trace_buffer == nullptr) return;

#ifdef CONFIG_VPMU_DEBUG_MSG
        debug_packet_num_cnt++;
        if (ref.type == VPMU_PACKET_DUMP_INFO) {
            CONSOLE_LOG("VPMU sent %'" PRIu64 " packets\n", debug_packet_num_cnt);
            debug_packet_num_cnt = 0;
        }
#endif

        for (int i = 0; i < num_workers; i++)
            while (shm_remainedBufferSpace(trace_buffer, i) <= 1) usleep(1);
        shm_bufferWrite(trace_buffer, // Pointer to ringbuffer
                        ref);         // The reference elements
        this->post_semaphore();       // up semaphores
    }

private:
    using VPMUStream_Impl<T>::log;
    using VPMUStream_Impl<T>::log_debug;
    using VPMUStream_Impl<T>::log_fatal;

    using VPMUStream_Impl<T>::platform_info;
    using VPMUStream_Impl<T>::buffer;
    using VPMUStream_Impl<T>::stream_comm;
    using VPMUStream_Impl<T>::heart_beat;
    using VPMUStream_Impl<T>::trace_buffer;
    using VPMUStream_Impl<T>::num_trace_buffer_elems;
    using VPMUStream_Impl<T>::token;
    using VPMUStream_Impl<T>::num_workers;

    boost::interprocess::shared_memory_object shm;
    boost::interprocess::mapped_region        region;
    std::vector<pid_t>                        slaves;
    std::thread                               heart_beat_thread;

    // The total number of packets counter for debugging
    uint64_t debug_packet_num_cnt = 0;

    void fork_zombie_killer()
    {
        pid_t parent_pid = getpid();
        pid_t pid        = fork();

        if (pid) {
            // Parent
            slaves.push_back(pid);
            heart_beat_thread = std::thread([=] {
                vpmu::utils::name_thread("heart beat");
                while (true) {
                    // Only be cancelable at cancellation points
                    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
                    usleep(100 * 1000); // 0.1 second
                    *heart_beat += 1;
                }
            });
        } else {
            volatile uint64_t last_heart_beat     = 0;
            bool              print_gdb_once_flag = false;

            vpmu::utils::name_process("zombie-killer");
            while (true) {
                usleep(500 * 1000); // 0.5 second
                if (*heart_beat == last_heart_beat) {
                    if (kill(parent_pid, 0)) {
                        if (errno == ESRCH) {
                            LOG_FATAL("QEMU stops beating... kill all zombies!!\n");
                            this->destroy();
                            log("Destructed\n");
                            // Exit without calling unnecessary destructor of global
                            // variables.
                            // This prevents double-free shared resources.
                            abort();
                        }
                    } else {
                        if (print_gdb_once_flag == false)
                            log_debug("QEMU stops beating... but still exist. "
                                      "Consider it as stopped by ptrace (gdb).\n");
                        print_gdb_once_flag = true;
                    }
                }
                last_heart_beat = *heart_beat;
            }
            // It should never return!!!
            abort();
        }
    }
};

#endif
