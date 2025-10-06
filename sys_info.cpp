#include "sys_info.hpp"
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

std::string exec(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) { throw std::runtime_error("popen() failed!"); }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }
    return result;
}

std::string getValueFromLine(const std::string& line, const std::string& key) {
    size_t key_pos = line.find(key);
    if (key_pos == std::string::npos) return "";
    std::string value = line.substr(key_pos + key.length());
    trim(value);
    return value;
}

CpuInfo getCpuInfo() {
    CpuInfo info; std::string lscpu_output = exec("lscpu"); std::stringstream ss(lscpu_output); std::string line;
    while(std::getline(ss, line)){
        if(line.rfind("Model name:", 0) == 0) info.model = getValueFromLine(line, "Model name:");
        if(line.rfind("Core(s) per socket:", 0) == 0) info.cores = getValueFromLine(line, "Core(s) per socket:");
        if(line.rfind("CPU(s):", 0) == 0) info.threads = getValueFromLine(line, "CPU(s):");
        if(line.rfind("CPU max MHz:", 0) == 0){
            try { float freq_mhz = std::stof(getValueFromLine(line, "CPU max MHz:")); std::stringstream freq_ss;
                freq_ss << std::fixed << std::setprecision(2) << freq_mhz / 1000.0 << " GHz"; info.max_freq = freq_ss.str();
            } catch (...) { info.max_freq = "N/A"; }
        }
    } return info;
}
MemoryInfo getMemoryInfo() {
    MemoryInfo info; std::ifstream file("/proc/meminfo"); std::string line; long mem_total = 0, mem_available = 0;
    while (std::getline(file, line)) { std::stringstream ss(line); std::string key; long value; ss >> key >> value;
        if (key == "MemTotal:") mem_total = value; else if (key == "MemAvailable:") mem_available = value;
    }
    if (mem_total > 0 && mem_available > 0) {
        long mem_used = mem_total - mem_available; float total_gb = mem_total / 1024.0 / 1024.0; float used_gb = mem_used / 1024.0 / 1024.0;
        std::stringstream ss_total, ss_used; ss_total << std::fixed << std::setprecision(2) << total_gb << " GB";
        ss_used << std::fixed << std::setprecision(2) << used_gb << " GB"; info.total = ss_total.str(); info.used = ss_used.str();
    } return info;
}
BoardInfo getBoardInfo() {
    BoardInfo info; info.manufacturer = exec("sudo dmidecode -s baseboard-manufacturer"); info.product_name = exec("sudo dmidecode -s baseboard-product-name");
    info.version = exec("sudo dmidecode -s baseboard-version"); info.serial_number = exec("sudo dmidecode -s baseboard-serial-number");
    trim(info.manufacturer); trim(info.product_name); trim(info.version); trim(info.serial_number);
    return info;
}
BiosInfo getBiosInfo() {
    BiosInfo info; info.vendor = exec("sudo dmidecode -s bios-vendor"); info.version = exec("sudo dmidecode -s bios-version");
    info.release_date = exec("sudo dmidecode -s bios-release-date");
    trim(info.vendor); trim(info.version); trim(info.release_date);
    return info;
}

void parseLspciVmm(const std::string& command, std::vector<PciDevice>& devices, const std::string& class_filter = "") {
    std::string output = exec(command.c_str());
    if (output.empty()) return;

    std::stringstream ss(output);
    std::string line;
    PciDevice current_device;
    std::string current_class;
    bool device_block_started = false;

    while (std::getline(ss, line)) {
        if (line.empty()) {
            if (device_block_started) {
                if (class_filter.empty() || (!current_class.empty() && current_class.find(class_filter) != std::string::npos)) {
                    devices.push_back(current_device);
                }
                current_device = {};
                current_class = "";
                device_block_started = false;
            }
        } else {
            device_block_started = true;
            if (line.rfind("Class:", 0) == 0) {
                current_class = getValueFromLine(line, "Class:");
            } else if (line.rfind("Vendor:", 0) == 0) {
                current_device.vendor = getValueFromLine(line, "Vendor:");
            } else if (line.rfind("Device:", 0) == 0) {
                current_device.name = getValueFromLine(line, "Device:");
            } else if (line.rfind("Driver:", 0) == 0) {
                current_device.driver = getValueFromLine(line, "Driver:");
            }
        }
    }
    if (device_block_started) {
        if (class_filter.empty() || (!current_class.empty() && current_class.find(class_filter) != std::string::npos)) {
            devices.push_back(current_device);
        }
    }
}

std::vector<PciDevice> getGpuDevices() {
    std::vector<PciDevice> devices;
    parseLspciVmm("lspci -vmm", devices, "VGA compatible controller");
    return devices;
}
std::string get_pci_slot_from_device_path(const std::string& path) {
    fs::path device_path(path);
    if (fs::exists(device_path) && fs::is_symlink(device_path)) {
        fs::path target_path = fs::read_symlink(device_path);
        if (target_path.string().find("pci") != std::string::npos) {
            return target_path.filename().string();
        }
    }
    return "";
}

std::vector<PciDevice> getAudioDevices() {
    std::vector<PciDevice> devices;
    const std::string sound_path = "/sys/class/sound/";

    if (!fs::exists(sound_path) || !fs::is_directory(sound_path)) {
        return devices;
    }

    for (const auto& entry : fs::directory_iterator(sound_path)) {
        if (entry.is_directory() && entry.path().filename().string().rfind("card", 0) == 0) {
            fs::path device_symlink = entry.path() / "device";
            std::string pci_slot = get_pci_slot_from_device_path(device_symlink.string());

            if (!pci_slot.empty()) {
                std::string cmd = "lspci -s " + pci_slot + " -vmm";
                parseLspciVmm(cmd, devices);
            }
        }
    }
    return devices;
}
std::vector<PciDevice> getNetworkDevices() {
    std::vector<PciDevice> devices;
    parseLspciVmm("lspci -vmm", devices, "Ethernet controller");
    parseLspciVmm("lspci -vmm", devices, "Network controller");
    return devices;
}
std::vector<MonitorInfo> getMonitorInfo() {
    std::vector<MonitorInfo> monitors;
    std::string xrandr_output = exec("xrandr --query");
    std::stringstream ss(xrandr_output);
    std::string line, current_monitor_name;
    while (std::getline(ss, line)) {
        if (line.find(" connected") != std::string::npos) {
            current_monitor_name = line.substr(0, line.find(" "));
        } else if (!current_monitor_name.empty() && line.find("   ") == 0 && line.find("*") != std::string::npos) {
             MonitorInfo mon; mon.name = current_monitor_name;
             std::stringstream res_ss(line); res_ss >> mon.resolution;
             monitors.push_back(mon);
             current_monitor_name = "";
        }
    }
    return monitors;
}