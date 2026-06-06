#include "bridge.h"
#include "config.h"
#include "http_server.h"
#include "metrics.h"
#include "simulator.h"
#include "telemetry.h"

#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

// =============================================================================
// main.cpp — Signal handler and program entry point
// =============================================================================

void on_signal(int)
{
    g_running.store(false);
    std::cout << "\n[Main] Ctrl+C received — shutting down (up to 1 second)...\n";
}

int main()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << "=======================================================\n";
    std::cout << " RoCEv2 Congestion Telemetry — Integrated Demo\n";
    std::cout << "=======================================================\n\n";

    try
    {
        // Open /dev/mock_nic — file descriptor into the kernel module
        DeviceHandle dev(DEVICE_PATH);

        // After this, shared_mem points to the same physical DRAM as
        // kernel_buffer in the module.
        MappedBuffer mapped(dev.fd(), MAP_SIZE);
        void *shared_mem = mapped.ptr();

        // All three threads operate on the same shared_mem pointer.
        SimulatorThread sim(shared_mem);
        TelemetryThread tel(shared_mem);
        HttpServer http(HTTP_PORT);

        sim.start();
        tel.start();
        http.start();

        std::cout << "\nAll components running.\n";
        std::cout << "Find your VM IP with: ip addr show | grep 'inet ' | grep -v '127.0.0.1'\n";
        std::cout << "Then open: http://YOUR_VM_IP:" << HTTP_PORT << "\n";
        std::cout << "Press Ctrl+C to shut down.\n\n";

        // Print a live status line every 2 seconds
        while (g_running.load())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::lock_guard<std::mutex> lk(g_metrics.mtx);
            double fill = (double)g_metrics.buffer_slots / g_metrics.buffer_capacity * 100.0;
            std::cout << "[Status] sender  rate=" << g_metrics.sender_rate_pps
                      << " pps  buf=" << std::fixed << std::setprecision(1)
                      << fill << "%"
                      << "  is ecn=" << (g_metrics.ecn_active ? "YES" : "no")
                      << "  cong over window=" << g_metrics.congestion_rate << "%"
                      << "  total pkts sent=" << g_metrics.total_sent << "\n";
        }

        sim.stop();
        tel.stop();
        http.stop();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "\n[FATAL] " << ex.what() << "\n";
        return 1;
    }

    std::cout << "[Main] Clean shutdown complete.\n";
    return 0;
}
