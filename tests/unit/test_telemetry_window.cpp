#include <gtest/gtest.h>
#include "telemetry.h"
#include "config.h"

TEST(TelemetryWindow, ZeroTotalReturnsZero) {
    EXPECT_DOUBLE_EQ(TelemetryThread::compute_congestion_rate(0, 0), 0.0);
}

TEST(TelemetryWindow, AllCongestedIs100Percent) {
    EXPECT_DOUBLE_EQ(TelemetryThread::compute_congestion_rate(1000, 1000), 100.0);
}

TEST(TelemetryWindow, NoneCongestedIsZero) {
    EXPECT_DOUBLE_EQ(TelemetryThread::compute_congestion_rate(0, 1000), 0.0);
}

TEST(TelemetryWindow, HalfCongestedIs50Percent) {
    EXPECT_DOUBLE_EQ(TelemetryThread::compute_congestion_rate(500, 1000), 50.0);
}

// Alert threshold is strict >: exactly at threshold must NOT fire
TEST(TelemetryWindow, ExactlyAtThresholdNoAlert) {
    double rate = TelemetryThread::compute_congestion_rate(
        static_cast<uint64_t>(ALERT_THRESHOLD_PCT), 100);
    EXPECT_FALSE(rate > ALERT_THRESHOLD_PCT);
}

TEST(TelemetryWindow, OneAboveThresholdFires) {
    double rate = TelemetryThread::compute_congestion_rate(
        static_cast<uint64_t>(ALERT_THRESHOLD_PCT) + 1, 100);
    EXPECT_TRUE(rate > ALERT_THRESHOLD_PCT);
}
