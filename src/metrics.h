#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>

#include "config.h"

#define MAX_RECENT_PACKETS 40
#define BUFFER_CAPACITY 200
#define RED_THRESHOLD_PCT 70

// =============================================================================
// metrics.h — Shared state between SimulatorThread, TelemetryThread, HttpServer
// =============================================================================


// To track stats for the last MAX_RECENT_PACKETS in UI
struct RecentPacket
{
    uint32_t seq;
    bool ecn;
    bool dropped; // not implemented yet
};

struct SharedMetrics
{
    std::mutex mtx;

    uint32_t sender_rate_pps = DEFAULT_RATE_PPS;
    bool congestion_event = false;

    // Switch buffer model
    int buffer_slots = 0;
    int buffer_capacity = BUFFER_CAPACITY;
    int red_threshold = (BUFFER_CAPACITY * RED_THRESHOLD_PCT) / 100;
    bool ecn_active = false;
    bool dropping = false;

    // Telemetry output
    double congestion_rate = 0.0;
    bool alert_active = false;

    // Cumulative counters
    uint64_t total_sent = 0;
    uint64_t total_received = 0;
    uint64_t total_congested = 0;
    uint64_t total_dropped = 0; // not implemented yet

    // Recent packet log for the UI's packet stream display
    std::deque<RecentPacket> recent;
    static constexpr int MAX_RECENT = MAX_RECENT_PACKETS;

    void push(RecentPacket p)
    {
        recent.push_front(p);
        if ((int)recent.size() > MAX_RECENT)
            recent.pop_back();
    }
};

// Global shared state
extern SharedMetrics g_metrics;
extern std::atomic<uint32_t> g_rate_pps;
extern std::atomic<bool> g_running;
