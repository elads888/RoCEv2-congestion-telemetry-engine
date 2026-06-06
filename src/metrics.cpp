#include "metrics.h"

// =============================================================================
// metrics.cpp — Definitions of shared global state
// =============================================================================

SharedMetrics g_metrics;
std::atomic<uint32_t> g_rate_pps{DEFAULT_RATE_PPS};
std::atomic<bool> g_running{true};
