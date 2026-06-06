#include <gtest/gtest.h>
#include "simulator.h"
#include "config.h"

static constexpr int CAP   = 200;
static constexpr int RED   = (CAP * 70) / 100;          // 140
static constexpr int TICK  = 10;                         // ms
static constexpr int DRAIN = DRAIN_RATE_PPS * TICK / 1000; // 50

TEST(BufferModel, BelowDrainBufferDrains) {
    // default rate 3000 pps → 30 pkts/tick < drain 50 → empty
    int pkts = DEFAULT_RATE_PPS * TICK / 1000;
    auto r = SimulatorThread::compute_buffer_tick(0, pkts, DRAIN, CAP, RED);
    EXPECT_EQ(r.slots, 0);
    EXPECT_FALSE(r.ecn);
    EXPECT_FALSE(r.dropping);
}

TEST(BufferModel, AboveDrainBufferFills) {
    // max rate 12000 pps → 120 pkts/tick, net +70
    int pkts = MAX_RATE_PPS * TICK / 1000;
    auto r = SimulatorThread::compute_buffer_tick(0, pkts, DRAIN, CAP, RED);
    EXPECT_EQ(r.slots, 70);
    EXPECT_FALSE(r.ecn);  // 70 < RED threshold 140
}

TEST(BufferModel, ECNTriggersExactlyAtRedThreshold) {
    // land one below threshold: no ECN
    auto below = SimulatorThread::compute_buffer_tick(0, RED - 1, 0, CAP, RED);
    EXPECT_EQ(below.slots, RED - 1);
    EXPECT_FALSE(below.ecn);
    // land exactly at threshold: ECN fires
    auto at = SimulatorThread::compute_buffer_tick(0, RED, 0, CAP, RED);
    EXPECT_EQ(at.slots, RED);
    EXPECT_TRUE(at.ecn);
}

TEST(BufferModel, DroppingTriggersAtCapacity) {
    auto r = SimulatorThread::compute_buffer_tick(CAP - 1, 2, 0, CAP, RED);
    EXPECT_EQ(r.slots, CAP);
    EXPECT_TRUE(r.dropping);
}

TEST(BufferModel, SlotsNeverGoNegative) {
    auto r = SimulatorThread::compute_buffer_tick(0, 1, 1000, CAP, RED);
    EXPECT_EQ(r.slots, 0);
}

TEST(BufferModel, SlotsNeverExceedCapacity) {
    auto r = SimulatorThread::compute_buffer_tick(CAP, 1000, 0, CAP, RED);
    EXPECT_EQ(r.slots, CAP);
}
