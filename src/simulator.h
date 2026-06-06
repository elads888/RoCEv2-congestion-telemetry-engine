#pragma once

#include <atomic>
#include <thread>

#include "rocev2_header.h"
#include "metrics.h"

// =============================================================================
// simulator.h — SimulatorThread: DMA engine replacement
// =============================================================================

class SimulatorThread
{
public:
    struct BufferTick { int slots; bool ecn; bool dropping; };
    static BufferTick compute_buffer_tick(int slots, int pkts_in, int drain, int capacity, int red_threshold);

    explicit SimulatorThread(void *mem);

    void start();
    void stop();
    ~SimulatorThread();

private:
    void run();

    void *mem_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};
