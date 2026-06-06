#include "http_server.h"
#include "config.h"
#include "metrics.h"
#include "ui_html.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// =============================================================================
// http_server.cpp — HTTP server implementation
// =============================================================================

static const std::string CORS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n";

static std::string build_state_json()
{
    std::lock_guard<std::mutex> lk(g_metrics.mtx);

    double fill_pct = (double)g_metrics.buffer_slots / g_metrics.buffer_capacity * 100.0;

    std::ostringstream pkts;
    pkts << "[";
    bool first = true;
    for (const auto &p : g_metrics.recent)
    {
        if (!first)
            pkts << ",";
        pkts << "{\"seq\":" << p.seq
             << ",\"ecn\":" << (p.ecn ? "true" : "false")
             << ",\"dropped\":" << (p.dropped ? "true" : "false")
             << "}";
        first = false;
    }
    pkts << "]";

    std::ostringstream j;
    j << std::fixed << std::setprecision(1);
    j << "{"
      << "\"sender_rate\":" << g_metrics.sender_rate_pps << ","
      << "\"drain_rate\":" << DRAIN_RATE_PPS << ","
      << "\"buffer_slots\":" << g_metrics.buffer_slots << ","
      << "\"buffer_capacity\":" << g_metrics.buffer_capacity << ","
      << "\"buffer_fill_pct\":" << fill_pct << ","
      << "\"ecn_active\":" << (g_metrics.ecn_active ? "true" : "false") << ","
      << "\"dropping\":" << (g_metrics.dropping ? "true" : "false") << ","
      << "\"congestion_rate\":" << g_metrics.congestion_rate << ","
      << "\"alert_active\":" << (g_metrics.alert_active ? "true" : "false") << ","
      << "\"total_sent\":" << g_metrics.total_sent << ","
      << "\"total_received\":" << g_metrics.total_received << ","
      << "\"total_congested\":" << g_metrics.total_congested << ","
      << "\"total_dropped\":" << g_metrics.total_dropped << ","
      << "\"recent_packets\":" << pkts.str()
      << "}";
    return j.str();
}

uint32_t HttpServer::parse_rate_from_body(const std::string& body) {
    size_t rp = body.find("\"rate\":");
    if (rp == std::string::npos) return 0;
    try { return std::stoul(body.substr(rp + 7)); } catch (...) { return 0; }
}

bool HttpServer::is_valid_rate(uint32_t rate) {
    return rate >= 200 && rate <= MAX_RATE_PPS;
}

static void handle_http_client(int cfd)
{
    char buf[8192] = {};
    int n = read(cfd, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        close(cfd);
        return;
    }

    std::string req(buf, n);
    std::string resp;

    // lambda to build HTTP response with CORS headers
    auto make_resp = [&](int code, const std::string &ct, const std::string &body)
    {
        std::string status = (code == 200) ? "200 OK" : "204 No Content";
        resp = "HTTP/1.1 " + status + "\r\n"
                                      "Content-Type: " +
               ct + "\r\n"
                    "Content-Length: " +
               std::to_string(body.size()) + "\r\n" + CORS + "\r\n" + body;
    };

    if (req.substr(0, 7) == "OPTIONS")
    {
        resp = "HTTP/1.1 204 No Content\r\n" + CORS + "Content-Length: 0\r\n\r\n";
    }
    else if (req.substr(0, 9) == "GET /stat")
    {
        make_resp(200, "application/json", build_state_json());
    }
    else if (req.substr(0, 9) == "POST /rat")
    {
        size_t pos = req.find("\r\n\r\n");
        if (pos != std::string::npos)
        {
            std::string body = req.substr(pos + 4);
            uint32_t new_rate = HttpServer::parse_rate_from_body(body);
            if (HttpServer::is_valid_rate(new_rate))
                g_rate_pps.store(new_rate);
        }
        make_resp(200, "application/json", "{}");
    }
    else
    {
        make_resp(200, "text/html; charset=utf-8", UI_HTML);
    }

    write(cfd, resp.c_str(), resp.size());
    close(cfd);
}

HttpServer::HttpServer(int port) : port_(port) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start()
{
    running_ = true;
    thread_ = std::thread(&HttpServer::run, this);
}

void HttpServer::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void HttpServer::run()
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(sfd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "[HTTP] bind() failed on port " << port_
                  << ": " << strerror(errno) << "\n";
        return;
    }

    listen(sfd, 32);

    // SO_RCVTIMEO gives accept() a 1-second timeout.
    // Without this, accept() blocks indefinitely and Ctrl+C cannot kill
    // the process cleanly — the HTTP thread never wakes up to check g_running.
    // With the timeout, accept() returns EAGAIN each second, the loop checks
    // g_running, and the thread exits within one second of Ctrl+C.
    struct timeval tv{1, 0};
    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::cout << "[HTTP] Server on http://localhost:" << port_
              << "  -  open http://YOUR_VM_IP:" << port_ << " in your browser\n";

    while (running_ && g_running.load())
    {
        sockaddr_in ca{};
        socklen_t cl = sizeof(ca);
        int cfd = accept(sfd, (sockaddr *)&ca, &cl);
        if (cfd < 0)
            continue; // timeout or error — check g_running
        std::cout << "[HTTP] Accepted connection from " << inet_ntoa(ca.sin_addr) << ":" << ntohs(ca.sin_port) << "\n";
        std::thread(handle_http_client, cfd).detach();
    }
    close(sfd);
}
