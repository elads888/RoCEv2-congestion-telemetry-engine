#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

// =============================================================================
// http_server.h — Minimal HTTP server
//
// Routes:
//   GET  /state  → JSON snapshot of SharedMetrics (polled by UI every 200ms)
//   POST /rate   → body {"rate": NNNN} sets g_rate_pps (from UI slider)
//   GET  /       → serves the embedded UI HTML
// =============================================================================

class HttpServer
{
public:
    static bool     is_valid_rate(uint32_t rate);
    static uint32_t parse_rate_from_body(const std::string& body);

	explicit HttpServer(int port);

	void start();
	void stop();
	~HttpServer();

private:
	void run();

	int port_;
	std::atomic<bool> running_{false};
	std::thread thread_;
};
