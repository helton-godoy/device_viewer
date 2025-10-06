#ifndef SYS_INFO_HPP
#define SYS_INFO_HPP

#include <string>
#include <vector>
#include <map>

struct CpuInfo { std::string model, cores, threads, max_freq; };
struct MemoryInfo { std::string total, used; };
struct BoardInfo { std::string manufacturer, product_name, version, serial_number; };
struct BiosInfo { std::string vendor, version, release_date; };
struct MonitorInfo {
    std::string name;
    std::string resolution;
};

struct PciDevice {
    std::string slot;
    std::string class_name;
    std::string name;
    std::string vendor;
    std::string driver;
    std::string modules;
    std::string irq;
    std::vector<std::string> regions;
    std::map<std::string, std::string> additional_attributes;
};

CpuInfo getCpuInfo();
MemoryInfo getMemoryInfo();
BoardInfo getBoardInfo();
BiosInfo getBiosInfo();
std::vector<MonitorInfo> getMonitorInfo();
std::vector<PciDevice> getAllPciDevices();
std::string exec(const char* cmd);

#endif // SYS_INFO_HPP