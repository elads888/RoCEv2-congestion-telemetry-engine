#include <gtest/gtest.h>
#include "http_server.h"
#include "config.h"

// --- is_valid_rate ---

TEST(RateValidator, BelowMinRejected) {
    EXPECT_FALSE(HttpServer::is_valid_rate(0));
    EXPECT_FALSE(HttpServer::is_valid_rate(199));
}

TEST(RateValidator, AboveMaxRejected) {
    EXPECT_FALSE(HttpServer::is_valid_rate(MAX_RATE_PPS + 1));
}

TEST(RateValidator, BoundariesAccepted) {
    EXPECT_TRUE(HttpServer::is_valid_rate(200));
    EXPECT_TRUE(HttpServer::is_valid_rate(MAX_RATE_PPS));
}

// --- parse_rate_from_body ---

TEST(RateParser, ExtractsRate) {
    EXPECT_EQ(HttpServer::parse_rate_from_body("{\"rate\":4000}"), 4000u);
}

TEST(RateParser, MissingFieldReturnsZero) {
    EXPECT_EQ(HttpServer::parse_rate_from_body("{}"), 0u);
}

TEST(RateParser, MalformedValueReturnsZero) {
    EXPECT_EQ(HttpServer::parse_rate_from_body("{\"rate\":abc}"), 0u);
}

TEST(RateParser, EmptyBodyReturnsZero) {
    EXPECT_EQ(HttpServer::parse_rate_from_body(""), 0u);
}
