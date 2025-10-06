#ifndef SYS_INFO_HPP
#define SYS_INFO_HPP

#include <string>
#include <vector>

struct CpuInfo { std::string model, cores, threads, max_freq; };
struct MemoryInfo { std::string total, used; };
struct BoardInfo { std::string manufacturer, product_name, version, serial_number; };
struct BiosInfo { std::string vendor, version, release_date; };
struct PciDevice {
    std::string name;
    std::string vendor;
    std::string driver;
    std::string bus = "PCI";
};
struct MonitorInfo {
    std::string name;
    std::string resolution;
};

CpuInfo getCpuInfo();
MemoryInfo getMemoryInfo();
BoardInfo getBoardInfo();
BiosInfo getBiosInfo();
std::vector<PciDevice> getGpuDevices();
std::vector<MonitorInfo> getMonitorInfo();
std::vector<PciDevice> getAudioDevices();
std::vector<PciDevice> getNetworkDevices();
std::string exec(const char* cmd);

#endif // SYS_INFO_HPP