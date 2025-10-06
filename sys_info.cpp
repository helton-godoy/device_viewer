#include "sys_info.hpp"
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <algorithm>
#include <cctype>

// --- Helper Functions ---
void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

std::string exec(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) { return ""; } // Return empty on error
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

// --- Standard Hardware Info (Non-PCI) ---
CpuInfo getCpuInfo() {
    CpuInfo info;
    std::string lscpu_output = exec("lscpu");
    std::stringstream ss(lscpu_output);
    std::string line;
    while(std::getline(ss, line)){
        if(line.rfind("Model name:", 0) == 0) info.model = getValueFromLine(line, "Model name:");
        if(line.rfind("Core(s) per socket:", 0) == 0) info.cores = getValueFromLine(line, "Core(s) per socket:");
        if(line.rfind("CPU(s):", 0) == 0) info.threads = getValueFromLine(line, "CPU(s):");
        if(line.rfind("CPU max MHz:", 0) == 0){
            try {
                float freq_mhz = std::stof(getValueFromLine(line, "CPU max MHz:"));
                std::stringstream freq_ss;
                freq_ss << std::fixed << std::setprecision(2) << freq_mhz / 1000.0 << " GHz";
                info.max_freq = freq_ss.str();
            } catch (...) { info.max_freq = "N/A"; }
        }
    }
    return info;
}

MemoryInfo getMemoryInfo() {
    MemoryInfo info;
    std::ifstream file("/proc/meminfo");
    std::string line;
    long mem_total = 0, mem_available = 0;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key;
        long value;
        ss >> key >> value;
        if (key == "MemTotal:") mem_total = value;
        else if (key == "MemAvailable:") mem_available = value;
    }
    if (mem_total > 0 && mem_available > 0) {
        long mem_used = mem_total - mem_available;
        float total_gb = mem_total / 1024.0 / 1024.0;
        float used_gb = mem_used / 1024.0 / 1024.0;
        std::stringstream ss_total, ss_used;
        ss_total << std::fixed << std::setprecision(2) << total_gb << " GB";
        ss_used << std::fixed << std::setprecision(2) << used_gb << " GB";
        info.total = ss_total.str();
        info.used = ss_used.str();
    }
    return info;
}

BoardInfo getBoardInfo() {
    BoardInfo info;
    info.manufacturer = exec("sudo dmidecode -s baseboard-manufacturer");
    info.product_name = exec("sudo dmidecode -s baseboard-product-name");
    info.version = exec("sudo dmidecode -s baseboard-version");
    info.serial_number = exec("sudo dmidecode -s baseboard-serial-number");
    trim(info.manufacturer); trim(info.product_name); trim(info.version); trim(info.serial_number);
    return info;
}

BiosInfo getBiosInfo() {
    BiosInfo info;
    info.vendor = exec("sudo dmidecode -s bios-vendor");
    info.version = exec("sudo dmidecode -s bios-version");
    info.release_date = exec("sudo dmidecode -s bios-release-date");
    trim(info.vendor); trim(info.version); trim(info.release_date);
    return info;
}

std::vector<MonitorInfo> getMonitorInfo() {
    std::vector<MonitorInfo> monitors;
    std::string xrandr_output = exec("xrandr --listmonitors");
    std::stringstream ss(xrandr_output);
    std::string line;
    std::getline(ss, line); // Skip header line
    while (std::getline(ss, line)) {
        trim(line);
        std::stringstream line_ss(line);
        std::string part;
        std::vector<std::string> parts;
        while(line_ss >> part) {
            parts.push_back(part);
        }
        if (parts.size() >= 4) {
            MonitorInfo mon;
            mon.name = parts.back();
            mon.resolution = parts[parts.size() - 2];
            monitors.push_back(mon);
        }
    }
    return monitors;
}

// --- PCI Device Collection (New Advanced Logic) ---

// Fetches additional attributes based on device class
void get_additional_attributes(PciDevice& device) {
    const std::string& class_name = device.class_name;

    // GPU Attributes
    if (class_name.find("VGA") != std::string::npos || class_name.find("3D") != std::string::npos) {
        std::string nvidia_smi_out = exec("nvidia-smi --query-gpu=memory.total,memory.used --format=csv,noheader,nounits");
        if (!nvidia_smi_out.empty()) {
            trim(nvidia_smi_out);
            device.additional_attributes["Memória GPU (MB)"] = nvidia_smi_out;
        }
    }

    // Storage Attributes
    if (class_name.find("SATA") != std::string::npos || class_name.find("Non-Volatile") != std::string::npos) {
        std::string lsblk_out = exec("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT -rn");
        if(!lsblk_out.empty()) {
             device.additional_attributes["Partições (lsblk)"] = lsblk_out;
        }

        std::string dev_node_str = exec("lsblk -ndo NAME,TYPE | awk '/disk/{print $1; exit}'");
        if (!dev_node_str.empty()) {
            trim(dev_node_str);
            std::string smart_cmd = "sudo smartctl -H /dev/" + dev_node_str;
            std::string smart_out = exec(smart_cmd.c_str());
            if(!smart_out.empty()){
                device.additional_attributes["SMART"] = getValueFromLine(smart_out, "SMART overall-health self-assessment test result:");
            }
        }
    }
}


std::vector<PciDevice> getAllPciDevices() {
    std::vector<PciDevice> devices;
    std::string lspci_vmm_out = exec("lspci -vmm");
    std::stringstream vmm_ss(lspci_vmm_out);
    std::string line;

    std::string current_slot;
    std::string current_class;

    while (std::getline(vmm_ss, line)) {
        if (line.rfind("Slot:", 0) == 0) {
            current_slot = getValueFromLine(line, "Slot:");
        } else if (line.rfind("Class:", 0) == 0) {
            current_class = getValueFromLine(line, "Class:");
        }

        if (line.empty() && !current_slot.empty() && !current_class.empty()) {
            PciDevice device;
            device.slot = current_slot;
            device.class_name = current_class;

            std::string lspci_details_cmd = "lspci -vvv -s " + current_slot;
            std::string details_out = exec(lspci_details_cmd.c_str());
            std::stringstream details_ss(details_out);

            std::string detail_line;
            std::getline(details_ss, detail_line); // First line is the name
            device.name = getValueFromLine(detail_line, current_slot);

            while (std::getline(details_ss, detail_line)) {
                if (detail_line.find("Subsystem:") != std::string::npos) {
                    device.vendor = getValueFromLine(detail_line, "Subsystem:");
                } else if (detail_line.find("Kernel driver in use:") != std::string::npos) {
                    device.driver = getValueFromLine(detail_line, "Kernel driver in use:");
                } else if (detail_line.find("Kernel modules:") != std::string::npos) {
                    device.modules = getValueFromLine(detail_line, "Kernel modules:");
                } else if (detail_line.find("Interrupt:") != std::string::npos) {
                    device.irq = getValueFromLine(detail_line, "Interrupt:");
                } else if (detail_line.find("Region ") != std::string::npos || detail_line.find("Expansion ROM") != std::string::npos) {
                    trim(detail_line);
                    device.regions.push_back(detail_line);
                }
            }

            get_additional_attributes(device);
            devices.push_back(device);

            current_slot = "";
            current_class = "";
        }
    }
    return devices;
}