// Include necessary headers for HTTP server and JSON handling
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <time.h>

#include <chrono>
#include <thread>

// Use nlohmann::json for JSON handling
using json = nlohmann::json;

// Include necessary headers for system information
#include "sys/types.h"
#include "sys/sysinfo.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "sys/times.h"

#include <sys/statvfs.h>
#include <cstring>

#include <iomanip>

#include <fstream>
#include <iostream>
#include <string>

int updateFrequency = 500; // Update frequency

int getUpdateFrequency() {
    return updateFrequency;
}

std::string readSharedContent() {
    std::ifstream file("/shared/output.json");

    if (!file) {
        return "";
    }

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

int main() {
    std::cout << readSharedContent() << std::endl;

    httplib::Server server;

    server.Get("/metrics/stream", [](const httplib::Request&, httplib::Response& res) {
        // Set headers
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "http://localhost");

        res.set_chunked_content_provider(
            "text/event-stream",
            [](size_t, httplib::DataSink& sink) {
                std::string content = readSharedContent();
                std::string contentMessage = content;
                json parsedContent = json::parse(content, nullptr, false);
                if (!parsedContent.is_discarded() && parsedContent.contains("message") && parsedContent["message"].is_string()) {
                    contentMessage = parsedContent["message"].get<std::string>();
                }

                auto start_time = std::chrono::steady_clock::now();
                // Calculate system metrics
                
                auto end_time = std::chrono::steady_clock::now();
                auto diff = end_time - start_time;
                auto diff_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(diff);
                json data = {
                    {"status", "connected"},
                    {"updateFrequency", getUpdateFrequency()},
                    {"timePerUpdate", diff_sec.count()},
                    {"content", contentMessage}
                };

                // Send data as a chunk
                std::string msg = "data: " + data.dump() + "\n\n";
                sink.write(msg.data(), msg.size());

                // Sleep for the specified update frequency
                std::this_thread::sleep_for(std::chrono::milliseconds(updateFrequency));
                return true;
            }
        );
    });

    server.listen("0.0.0.0", 80);
}
