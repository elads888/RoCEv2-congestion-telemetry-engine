#pragma once

#include <atomic>
#include <thread>

#include "rocev2_header.h"
#include "metrics.h"

// =============================================================================
// telemetry.h — TelemetryThread: congestion telemetry engine
// =============================================================================

class TelemetryThread
{
public:
    static double compute_congestion_rate(uint64_t congested, uint64_t total);

    explicit TelemetryThread(void *mem);

    void start();
    void stop();
    ~TelemetryThread();

private:
    void run();

    void *mem_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};
