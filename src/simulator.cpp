#include "simulator.h"
#include "config.h"

#include <algorithm>
#include <chrono>
#include <iostream>

// =============================================================================
// simulator.cpp — SimulatorThread implementation
//
// Mimics the switch buffer - the queue that builds up when packets arrive 
// faster than they can be forwarded, and the NIC's DMA engine that continuously
// writes packets to shared memory.
//
// PROBLEM: To model a buffer, we need to know how many packets arrived and
// how many drained in any given moment. Both of those are rates -
// (packets/time), so we need to measure time. Without it, there is no rate, 
// no fill level, no threshold crossing, and no congestion signal.
// A naive tight loop that just sends packets as fast as possible cannot measure time,
// it iterates as fast as the CPU allows with no knowledge of how
// much real time has elapsed. If we ask "how many packets arrived in the last
// 10ms?" (rate), we cannot answer, because we never measured 10ms.
//
// SOLUTION: The simulator advances in fixed 10ms ticks. Each tick represents
// exactly 10ms of simulated network time. We compute packets arrived and
// drained in that window, update the buffer, evaluate ECN, and write a
// descriptor to shared memory.
//
// =============================================================================

SimulatorThread::BufferTick SimulatorThread::compute_buffer_tick(int slots, int pkts_in, int drain, int cap, int red) {
    int s = std::clamp(slots + pkts_in - drain, 0, cap);
    return {s, s >= red, s >= cap};
}

SimulatorThread::SimulatorThread(void *mem) : mem_(mem) {}

SimulatorThread::~SimulatorThread() { stop(); }

void SimulatorThread::start()
{
    running_ = true;
    thread_ = std::thread(&SimulatorThread::run, this);
    std::cout << "[Simulator] DMA simulator started" << std::endl;
}

void SimulatorThread::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void SimulatorThread::run()
{
    auto *hdr = reinterpret_cast<RoCEv2_Header *>(mem_);

    uint32_t seq = 0;
    bool was_ecn = false;       // tracks transitions from past ticks for log output
    constexpr int TICK_MS = 10; // tick every 10ms makes the slider changes feel instant

    while (running_)
    {
        // read the current sender rate set by the HTTP server thread
        uint32_t rate = g_rate_pps.load();

        // ------------------------------------------------------------------
        // UPDATE THE SWITCH BUFFER MODEL
        //
        // buffer_slots represents the fill level of the switch's packet
        // buffer. It increases by the number of incoming packets this tick
        // and decreases by the number the switch can drain.
        //
        // pkts_this_tick < drain_this_tick  ->  buffer empties  ->  no ECN
        // pkts_this_tick > drain_this_tick  ->  buffer fills    ->  ECN triggers
        // pkts_this_tick = drain_this_tick  ->  buffer stable   ->  no ECN
        // ------------------------------------------------------------------
        int pkts_this_tick = std::max(1, (int)(rate * TICK_MS / 1000));
        int drain_this_tick = DRAIN_RATE_PPS * TICK_MS / 1000;

        // use of on stack variable to avoid usage of g_metrics because there can be concurrent telemetry thread and http server thread in the future
        bool ecn_now = false;
        {
            // std::lock_guard acquires g_metrics.mtx on construction and
            // releases it automatically when 'lk' goes out of scope at the
            // closing brace below (RAII).
            std::lock_guard<std::mutex> lk(g_metrics.mtx);
            
            // read global variable once, and then use stack variable for efficiency
            int cap = g_metrics.buffer_capacity;

            g_metrics.buffer_slots = std::clamp(
                g_metrics.buffer_slots + pkts_this_tick - drain_this_tick,
                0, cap);

            g_metrics.ecn_active = (g_metrics.buffer_slots >= g_metrics.red_threshold);
            g_metrics.dropping = (g_metrics.buffer_slots >= cap);
            g_metrics.sender_rate_pps = rate;
            g_metrics.total_sent += pkts_this_tick;
            ecn_now = g_metrics.ecn_active;

            // if ecn state changed since last tick
            if (ecn_now && !was_ecn)
            {
                std::cout << "[Switch] ECN threshold crossed — buffer="
                          << g_metrics.buffer_slots << "/" << cap
                          << " (" << (g_metrics.buffer_slots * 100 / cap) << "%)"
                          << " — congestion_flag=true" << std::endl;
                g_metrics.congestion_event = true;
            }
            else if (!ecn_now && was_ecn)
            {
                std::cout << "[Switch] Buffer drained below ECN threshold — " << std::endl;
                std::cout << "congestion_flag back to false" << std::endl;
            }
        }
        was_ecn = ecn_now;

        // ------------------------------------------------------------------
        // WRITE PACKET DESCRIPTORS INTO SHARED PHYSICAL MEMORY
        //
        // Zero-copy write.
        // The CPU executes these as normal store instructions in Ring 3.
        //
        // NOTICE - DOORBELL PROTOCOL:
        //   congestion_flag is always written before sequence_number is incremented.
        //   The TelemetryThread spins on sequence_number and only acts when it
        //   changes - by which point congestion_flag is already written. Reversing
        //   the order would be a race condition.
        // 
        // congestion_flag -> written first (data)
        // sequence_number -> written last  (doorbell signal)
        // ------------------------------------------------------------------
        for (int i = 0; i < pkts_this_tick; i++)
        {
            hdr->congestion_flag = ecn_now ? 1 : 0;
            hdr->sequence_number = ++seq;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(TICK_MS));
    }
}
