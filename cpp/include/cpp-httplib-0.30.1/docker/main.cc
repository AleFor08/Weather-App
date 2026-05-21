//
//  main.cc
//
//  Copyright (c) 2026 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <atomic>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>

#include <httplib.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#endif
#include <thread> 


#ifdef _WIN32
#include <psapi.h>
#include <TCHAR.h>
#include <pdh.h>


static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;

static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;

double getCPUValue();
double getCurrentCPUValue();
double getVirtualRam();
double getUsedVirtualRam();
double getVirtualRamProcess();
double getPhysicalRam();
double getUsedPhysicalRam();
double getPhysicalRamProcess();



void init(){
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    // You can also use L"\\Processor(*)\\% Processor Time" and get individual CPU values with PdhGetFormattedCounterArray()
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);

    SYSTEM_INFO sysInfo;
    FILETIME ftime, fsys, fuser;

    GetSystemInfo(&sysInfo);
    numProcessors = sysInfo.dwNumberOfProcessors;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU, &ftime, sizeof(FILETIME));

    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
}

double getCPUValue(){
    PDH_FMT_COUNTERVALUE counterVal;

    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    return counterVal.doubleValue;
}

double getCurrentCPUValue(){
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    percent = (sys.QuadPart - lastSysCPU.QuadPart) +
        (user.QuadPart - lastUserCPU.QuadPart);
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;

    return percent * 100;
}


double getVirtualRam(){
  // Total virtual memory
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;

  return (double)totalVirtualMem;
}

double getUsedVirtualRam(){
  // Used virtual memory
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile; 

  return (double)virtualMemUsed;
}

double getVirtualRamProcess(){
  // Process memory
  PROCESS_MEMORY_COUNTERS_EX pmc;
  GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
  SIZE_T virtualMemUsedProcess = pmc.PrivateUsage;

  return (double)virtualMemUsedProcess;
}


double getPhysicalRam(){
  // Total physical memory
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  DWORDLONG totalPhysMem = memInfo.ullTotalPhys;

  return (double)totalPhysMem;
}

double getUsedPhysicalRam(){
  // Total physical memory used
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

  return (double)physMemUsed;
}

double getPhysicalRamProcess(){
  // Physical memory used by process
  PROCESS_MEMORY_COUNTERS_EX pmc;
  GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
  SIZE_T physMemUsedProcess = pmc.WorkingSetSize;

  return (double)physMemUsedProcess;
}
#endif

using json = nlohmann::json;
using namespace httplib;

const auto SERVER_NAME =
    std::format("cpp-httplib-server/{}", CPPHTTPLIB_VERSION);

Server svr;

// Forward declarations for platform-specific metrics functions
int getCpuUsagePercent();
int getRamUsagePercent();

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cout << "\nReceived signal, shutting down gracefully...\n";
    svr.stop();
  }
}

std::string get_time_format() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%d/%b/%Y:%H:%M:%S %z");
  return ss.str();
}

std::string get_error_time_format() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y/%m/%d %H:%M:%S");
  return ss.str();
}

// NGINX Combined log format:
// $remote_addr - $remote_user [$time_local] "$request" $status $body_bytes_sent
// "$http_referer" "$http_user_agent"
void nginx_access_logger(const Request &req, const Response &res) {
  std::string remote_user =
      "-"; // cpp-httplib doesn't have built-in auth user tracking
  auto time_local = get_time_format();
  auto request = std::format("{} {} {}", req.method, req.path, req.version);
  auto status = res.status;
  auto body_bytes_sent = res.body.size();
  auto http_referer = req.get_header_value("Referer");
  if (http_referer.empty()) http_referer = "-";
  auto http_user_agent = req.get_header_value("User-Agent");
  if (http_user_agent.empty()) http_user_agent = "-";

  std::cout << std::format("{} - {} [{}] \"{}\" {} {} \"{}\" \"{}\"",
                           req.remote_addr, remote_user, time_local, request,
                           status, body_bytes_sent, http_referer,
                           http_user_agent)
            << std::endl;
}

// NGINX Error log format:
// YYYY/MM/DD HH:MM:SS [level] message, client: client_ip, request: "request",
// host: "host"
void nginx_error_logger(const Error &err, const Request *req) {
  auto time_local = get_error_time_format();
  std::string level = "error";

  if (req) {
    auto request =
        std::format("{} {} {}", req->method, req->path, req->version);
    auto host = req->get_header_value("Host");
    if (host.empty()) host = "-";

    std::cerr << std::format("{} [{}] {}, client: {}, request: "
                             "\"{}\", host: \"{}\"",
                             time_local, level, to_string(err),
                             req->remote_addr, request, host)
              << std::endl;
  } else {
    // If no request context, just log the error
    std::cerr << std::format("{} [{}] {}", time_local, level, to_string(err))
              << std::endl;
  }
}

void print_usage(const char *program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --host <hostname>        Server hostname (default: localhost)"
            << std::endl;
  std::cout << "  --port <port>            Server port (default: 8080)"
            << std::endl;
  std::cout << "  --mount <mount:path>     Mount point and document root"
            << std::endl;
  std::cout << "                           Format: mount_point:document_root"
            << std::endl;
  std::cout << "                           (default: /:./html)" << std::endl;
  std::cout << "  --trusted-proxy <ip>     Add trusted proxy IP address"
            << std::endl;
  std::cout << "                           (can be specified multiple times)"
            << std::endl;
  std::cout << "  --version                Show version information"
            << std::endl;
  std::cout << "  --help                   Show this help message" << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << program_name
            << " --host localhost --port 8080 --mount /:./html" << std::endl;
  std::cout << "  " << program_name
            << " --host 0.0.0.0 --port 3000 --mount /api:./api" << std::endl;
  std::cout << "  " << program_name
            << " --trusted-proxy 192.168.1.100 --trusted-proxy 10.0.0.1"
            << std::endl;
}

struct ServerConfig {
  std::string hostname = "localhost";
  int port = 8080;
  std::string mount_point = "/";
  std::string document_root = "./html";
  std::vector<std::string> trusted_proxies;
};

enum class ParseResult { SUCCESS, HELP_REQUESTED, VERSION_REQUESTED, ERROR };

ParseResult parse_command_line(int argc, char *argv[], ServerConfig &config) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return ParseResult::HELP_REQUESTED;
    } else if (strcmp(argv[i], "--host") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --host requires a hostname argument" << std::endl;
        print_usage(argv[0]);
        return ParseResult::ERROR;
      }
      config.hostname = argv[++i];
    } else if (strcmp(argv[i], "--port") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --port requires a port number argument"
                  << std::endl;
        print_usage(argv[0]);
        return ParseResult::ERROR;
      }
      config.port = std::atoi(argv[++i]);
      if (config.port <= 0 || config.port > 65535) {
        std::cerr << "Error: Invalid port number. Must be between 1 and 65535"
                  << std::endl;
        return ParseResult::ERROR;
      }
    } else if (strcmp(argv[i], "--mount") == 0) {
      if (i + 1 >= argc) {
        std::cerr
            << "Error: --mount requires mount_point:document_root argument"
            << std::endl;
        print_usage(argv[0]);
        return ParseResult::ERROR;
      }
      std::string mount_arg = argv[++i];
      auto colon_pos = mount_arg.find(':');
      if (colon_pos == std::string::npos) {
        std::cerr << "Error: --mount argument must be in format "
                     "mount_point:document_root"
                  << std::endl;
        print_usage(argv[0]);
        return ParseResult::ERROR;
      }
      config.mount_point = mount_arg.substr(0, colon_pos);
      config.document_root = mount_arg.substr(colon_pos + 1);

      if (config.mount_point.empty() || config.document_root.empty()) {
        std::cerr
            << "Error: Both mount_point and document_root must be non-empty"
            << std::endl;
        return ParseResult::ERROR;
      }
    } else if (strcmp(argv[i], "--version") == 0) {
      std::cout << CPPHTTPLIB_VERSION << std::endl;
      return ParseResult::VERSION_REQUESTED;
    } else if (strcmp(argv[i], "--trusted-proxy") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --trusted-proxy requires an IP address argument"
                  << std::endl;
        print_usage(argv[0]);
        return ParseResult::ERROR;
      }
      config.trusted_proxies.push_back(argv[++i]);
    } else {
      std::cerr << "Error: Unknown option '" << argv[i] << "'" << std::endl;
      print_usage(argv[0]);
      return ParseResult::ERROR;
    }
  }
  return ParseResult::SUCCESS;
}

bool setup_server(Server &svr, const ServerConfig &config) {
  svr.set_logger(nginx_access_logger);
  svr.set_error_logger(nginx_error_logger);

  // Set trusted proxies if specified
  if (!config.trusted_proxies.empty()) {
    svr.set_trusted_proxies(config.trusted_proxies);
  }

  auto ret = svr.set_mount_point(config.mount_point, config.document_root);
  if (!ret) {
    std::cerr
        << std::format(
               "Error: Cannot mount '{}' to '{}'. Directory may not exist.",
               config.mount_point, config.document_root)
        << std::endl;
    return false;
  }

  svr.set_file_extension_and_mimetype_mapping("html", "text/html");
  svr.set_file_extension_and_mimetype_mapping("htm", "text/html");
  svr.set_file_extension_and_mimetype_mapping("css", "text/css");
  svr.set_file_extension_and_mimetype_mapping("js", "text/javascript");
  svr.set_file_extension_and_mimetype_mapping("json", "application/json");
  svr.set_file_extension_and_mimetype_mapping("xml", "application/xml");
  svr.set_file_extension_and_mimetype_mapping("png", "image/png");
  svr.set_file_extension_and_mimetype_mapping("jpg", "image/jpeg");
  svr.set_file_extension_and_mimetype_mapping("jpeg", "image/jpeg");
  svr.set_file_extension_and_mimetype_mapping("gif", "image/gif");
  svr.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
  svr.set_file_extension_and_mimetype_mapping("ico", "image/x-icon");
  svr.set_file_extension_and_mimetype_mapping("pdf", "application/pdf");
  svr.set_file_extension_and_mimetype_mapping("zip", "application/zip");
  svr.set_file_extension_and_mimetype_mapping("txt", "text/plain");

  // Metrics endpoint with CORS support for the frontend
  svr.Options("/metrics", [](const Request&, Response &res) {
    res.set_header("Access-Control-Allow-Origin", "http://localhost");
    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.status = 204;
  });



  // getRAMValue();

  svr.Get("/metrics", [](const Request&, Response& res) {
    json data = {
      // {"cpu", getCPUValue()},
      // {"cpu current", getCurrentCPUValue()},
      // {"ram", totalVirtualMem == 0 ? 0 : (double)virtualMemUsed * 100 / totalVirtualMem},
      // {"ram process", totalVirtualMem == 0 ? 0 : (double)virtualMemUsedProcess * 100 / totalVirtualMem},
      // {"phys ram", totalPhysMem == 0 ? 0 : (double)physMemUsed * 100 / totalPhysMem},
      // {"phys ram process", totalPhysMem == 0 ? 0 : (double)physMemUsedProcess * 100 / totalPhysMem}
      {"cpu", getCpuUsagePercent()},
      {"ram", getRamUsagePercent()}
    };

    res.set_header("Access-Control-Allow-Origin", "http://localhost");
    res.set_content(data.dump(), "application/json");
  });

  // SSE: stream metrics continuously every 500ms
  svr.Get("/metrics/stream", [](const Request& /*req*/, Response &res) {
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "http://localhost");

    res.set_chunked_content_provider("text/event-stream",
      [](size_t /*offset*/, httplib::DataSink &sink) {
        // send one SSE event per call, sleep for 500ms then allow continuing
        json data = {
          {"cpu", getCpuUsagePercent()},
          {"ram", getRamUsagePercent()}
          // {"cpu", getCPUValue()},
          // {"cpu current", getCurrentCPUValue()},
          // {"ram", getVirtualRam()},
          // {"ram used", getUsedVirtualRam()},
          // {"ram process", getVirtualRamProcess()},
          // {"phys ram", getPhysicalRam()},
          // {"phys ram used", getUsedPhysicalRam()},
          // {"phys ram process", getPhysicalRamProcess()}
        };
        std::string s = "data: " + data.dump() + "\n\n";
        sink.write(s.data(), s.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // return true to indicate we'll send more data later
        return true;
      });
  });





  svr.set_error_handler([](const Request & /*req*/, Response &res) {
    if (res.status == 404) {
      res.set_content(
          std::format(
              "<html><head><title>404 Not Found</title></head>"
              "<body><h1>404 Not Found</h1>"
              "<p>The requested resource was not found on this server.</p>"
              "<hr><p>{}</p></body></html>",
              SERVER_NAME),
          "text/html");
    }
  });

  svr.set_pre_routing_handler([](const Request & /*req*/, Response &res) {
    res.set_header("Server", SERVER_NAME);
    return Server::HandlerResponse::Unhandled;
  });

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  return true;
}


#ifdef _WIN32

struct CpuTimes {
    ULONGLONG idle;
    ULONGLONG total;
};

CpuTimes readCpuTimes() {
    FILETIME idleTime, kernelTime, userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);

    ULONGLONG idle =
        ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;

    ULONGLONG kernel =
        ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;

    ULONGLONG user =
        ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;

    return { idle, kernel + user };
}

int getCpuUsagePercent() {
    auto t1 = readCpuTimes();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto t2 = readCpuTimes();

    ULONGLONG idle = t2.idle - t1.idle;
    ULONGLONG total = t2.total - t1.total;

    return (int)(100 * (total - idle) / total);
}

int getRamUsagePercent() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);

    DWORDLONG total = memInfo.ullTotalPhys;
    DWORDLONG available = memInfo.ullAvailPhys;

    return (int)((total - available) * 100 / total);
}

#else // POSIX / Linux

struct CpuTimes {
    unsigned long long idle;
    unsigned long long total;
};

CpuTimes readCpuTimes() {
    std::ifstream stat("/proc/stat");
    std::string line;
    std::getline(stat, line);
    std::istringstream ss(line);
    std::string cpu;
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    return { idle, total };
}

int getCpuUsagePercent() {
    auto t1 = readCpuTimes();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto t2 = readCpuTimes();

    unsigned long long idle = t2.idle - t1.idle;
    unsigned long long total = t2.total - t1.total;

    if (total == 0) return 0;
    return (int)(100 * (total - idle) / total);
}

int getRamUsagePercent() {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    unsigned long long total = memInfo.totalram;
    unsigned long long free = memInfo.freeram + memInfo.bufferram;
    unsigned long long used = (total > free) ? (total - free) : 0;
    if (total == 0) return 0;
    return (int)(used * 100 / total);
}

#endif



int main(int argc, char *argv[]) {
  ServerConfig config;

  auto result = parse_command_line(argc, argv, config);
  switch (result) {
  case ParseResult::HELP_REQUESTED:
  case ParseResult::VERSION_REQUESTED: return 0;
  case ParseResult::ERROR: return 1;
  case ParseResult::SUCCESS: break;
  }

  if (!setup_server(svr, config)) { return 1; }

  std::cout << "Serving HTTP on " << config.hostname << ":" << config.port
            << std::endl;
  std::cout << "Mount point: " << config.mount_point << " -> "
            << config.document_root << std::endl;

  if (!config.trusted_proxies.empty()) {
    std::cout << "Trusted proxies: ";
    for (size_t i = 0; i < config.trusted_proxies.size(); ++i) {
      if (i > 0) std::cout << ", ";
      std::cout << config.trusted_proxies[i];
    }
    std::cout << std::endl;
  }

  std::cout << "Press Ctrl+C to shutdown gracefully..." << std::endl;

  auto ret = svr.listen(config.hostname, config.port);

  std::cout << "Server has been shut down." << std::endl;

  return ret ? 0 : 1;
}
