#include "telemetry.h"
#include "config.h"

#include <chrono>
#include <iomanip>
#include <iostream>

// =============================================================================
// telemetry.cpp — TelemetryThread implementation
//
// Busy-polls sequence_number for new packets. Every WINDOW_MS evaluates
// the congestion rate. Fires an alert if rate > ALERT_THRESHOLD_PCT.
//
// The question we are handling is: Over a some period of time, what fraction of traffic was congested?
// =============================================================================

double TelemetryThread::compute_congestion_rate(uint64_t congested, uint64_t total) {
    return (total > 0) ? (100.0 * congested / total) : 0.0;
}

TelemetryThread::TelemetryThread(void *mem) : mem_(mem) {}

TelemetryThread::~TelemetryThread() { stop(); }

void TelemetryThread::start()
{
    running_ = true;
    thread_ = std::thread(&TelemetryThread::run, this);
    std::cout << "[Telemetry] Engine started — window=" << WINDOW_MS
              << "ms, threshold=" << ALERT_THRESHOLD_PCT << "%\n";
}

void TelemetryThread::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void TelemetryThread::run()
{
    // Use volatile so each poll checks memory.
    // Without it, the compiler may cache sequence_number in a register and miss packet updates.
    const auto *hdr = reinterpret_cast<volatile RoCEv2_Header *>(mem_);

    // creating alias
    using clock = std::chrono::steady_clock;

    uint32_t last_seq = hdr->sequence_number;
    uint64_t window_total = 0;
    uint64_t window_congested = 0;
    auto window_start = clock::now();

    while (running_)
    {
        // Busy-poll: spin until sequence_number changes.
        // detecting the packets the simulator writes.
        uint32_t cur;
        do
        {
            cur = hdr->sequence_number;
        } while (cur == last_seq && running_);

        // making sure not stopped by others
        if (!running_)
            break;

        uint32_t delta = cur - last_seq; 
        last_seq = cur;

        bool is_ecn = (hdr->congestion_flag == 1);
        window_total += delta;
        window_congested += is_ecn ? delta : 0;

        // same as in simulator thread - using lock guard for RAII style locking
        {
            std::lock_guard<std::mutex> lk(g_metrics.mtx);
            g_metrics.total_received += delta;
            g_metrics.total_congested += is_ecn ? delta : 0;

            // 
            g_metrics.push({cur, is_ecn, false}); // dropped not implemented yet - always false
        }

        // Evaluate the time window every WINDOW_MS milliseconds.
        // Anything above 20% over 100ms means DCQCN (slowing down) has failed to drain the congestion.
        auto now = clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - window_start)
                           .count();

        if (elapsed >= WINDOW_MS)
        {
            // Calculate congestion rate as a percentage.
            double congestion_rate = (window_total > 0)
                              ? (100.0 * window_congested / window_total)
                              : 0.0;

            {
                std::lock_guard<std::mutex> lk(g_metrics.mtx);
                g_metrics.congestion_rate = congestion_rate;
                g_metrics.alert_active = (congestion_rate > ALERT_THRESHOLD_PCT);
            }

            if (congestion_rate > ALERT_THRESHOLD_PCT)
            {
                std::cout << "[Telemetry] *** ALERT *** congestion_rate="
                          << std::fixed << std::setprecision(1) << congestion_rate
                          << "% > " << ALERT_THRESHOLD_PCT << "% threshold\n";
            }

            window_total = 0;
            window_congested = 0;
            window_start = now;
        }
    }
}
