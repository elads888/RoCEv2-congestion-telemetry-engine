#include <cstddef>
#include <gtest/gtest.h>
#include "config.h"
#include "rocev2_header.h"

// Compile-time: a reorder or packing change is caught at build, not runtime
static_assert(offsetof(RoCEv2_Header, sequence_number) == 0);
static_assert(offsetof(RoCEv2_Header, congestion_flag)  == 4);

TEST(HeaderLayout, SequenceNumberAtOffset0) {
    EXPECT_EQ(offsetof(RoCEv2_Header, sequence_number), 0u);
}

TEST(HeaderLayout, CongestionFlagAtOffset4) {
    EXPECT_EQ(offsetof(RoCEv2_Header, congestion_flag), 4u);
}

// Doorbell protocol: flag (data) and seq (doorbell) are independent fields.
// Changing one must not disturb the other.
TEST(HeaderLayout, FlagAndSeqAreIndependent) {
    RoCEv2_Header hdr{};
    hdr.congestion_flag  = 1;
    hdr.sequence_number  = 42;
    hdr.sequence_number  = 43;           // advance doorbell
    EXPECT_EQ(hdr.congestion_flag, 1u);  // flag unchanged
    hdr.congestion_flag = 0;
    EXPECT_EQ(hdr.sequence_number, 43u); // seq unchanged
}

TEST(HeaderLayout, FitsInMappedPage) {
    EXPECT_LE(sizeof(RoCEv2_Header), MAP_SIZE);
}
