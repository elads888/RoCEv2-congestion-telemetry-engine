#include <gtest/gtest.h>
#include "metrics.h"

TEST(MetricsRing, PushInsertsAtFront) {
    SharedMetrics m;
    m.push({10, false, false});
    m.push({20, true,  false});
    EXPECT_EQ(m.recent.front().seq, 20u);
    EXPECT_EQ(m.recent.back().seq,  10u);
}

TEST(MetricsRing, CapsAt40) {
    SharedMetrics m;
    for (int i = 0; i < 50; ++i)
        m.push({static_cast<uint32_t>(i), false, false});
    EXPECT_EQ(m.recent.size(), static_cast<size_t>(SharedMetrics::MAX_RECENT));
}

TEST(MetricsRing, EvictsOldestOnOverflow) {
    SharedMetrics m;
    for (int i = 0; i < 40; ++i)
        m.push({static_cast<uint32_t>(i), false, false});
    m.push({999, false, false});
    EXPECT_EQ(m.recent.front().seq, 999u);
    EXPECT_EQ(m.recent.back().seq,  1u);  // seq=0 evicted
}

TEST(MetricsRing, PreservesEcnFlag) {
    SharedMetrics m;
    m.push({1, true, false});
    EXPECT_TRUE(m.recent.front().ecn);
}

TEST(MetricsRing, PreservesDroppedFlag) {
    SharedMetrics m;
    m.push({1, false, true});
    EXPECT_TRUE(m.recent.front().dropped);
}
