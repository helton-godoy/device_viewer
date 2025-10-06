#include <ncurses.h>
#include <string>
#include <locale.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>

#include "sys_info.hpp"

#define COLOR_PAIR_HEADER 1
#define COLOR_PAIR_TITLE  2
#define COLOR_PAIR_VALUE  3
#define COLOR_PAIR_HINT   4
#define COLOR_PAIR_GREEN  5
#define COLOR_PAIR_CYAN   6
#define COLOR_PAIR_YELLOW 7
#define COLOR_PAIR_MAGENTA 8
#define COLOR_PAIR_BLUE    9
#define COLOR_PAIR_RED     10


// --- Helper Functions for Drawing ---
const int LABEL_WIDTH = 20;

// Wraps long text into multiple lines
std::vector<std::string> wrap_text(const std::string& text, int max_width) {
    std::vector<std::string> lines;
    if (text.empty() || max_width <= 0) {
        lines.push_back("");
        return lines;
    }
    std::stringstream word_stream(text);
    std::string word, current_line;
    while (word_stream >> word) {
        if ((int)(current_line.length() + word.length() + 1) > max_width) {
            lines.push_back(current_line);
            current_line = word;
        } else {
            if (!current_line.empty()) {
                current_line += " ";
            }
            current_line += word;
        }
    }
    if (!current_line.empty()) {
        lines.push_back(current_line);
    }
    if (lines.empty()) lines.push_back("");
    return lines;
}

void print_kv(WINDOW* pad, int& y, const std::string& label, const std::string& value, int color_pair = COLOR_PAIR_VALUE) {
    int value_col_width = getmaxx(pad) - LABEL_WIDTH - 3;

    std::vector<std::string> value_lines = wrap_text(value, value_col_width);

    // Print first line with label
    wattron(pad, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(pad, y, 2, "%-*s", LABEL_WIDTH - 2, (label + ":").c_str());
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_TITLE));

    wattron(pad, COLOR_PAIR(color_pair));
    mvwprintw(pad, y, LABEL_WIDTH, value_lines[0].c_str());
    wattroff(pad, COLOR_PAIR(color_pair));
    y++;

    // Print subsequent lines without label
    for (size_t i = 1; i < value_lines.size(); ++i) {
        wattron(pad, COLOR_PAIR(color_pair));
        mvwprintw(pad, y, LABEL_WIDTH, value_lines[i].c_str());
        wattroff(pad, COLOR_PAIR(color_pair));
        y++;
    }
}

void print_bullets(WINDOW* pad, int& y, const std::string& label, const std::vector<std::string>& items) {
    if (items.empty()) return;

    print_kv(pad, y, label, "");
    y--; // Adjust y back because print_kv increments it

    for (const auto& item : items) {
        wattron(pad, COLOR_PAIR(COLOR_PAIR_YELLOW));
        mvwprintw(pad, y, LABEL_WIDTH, "• ");
        wattroff(pad, COLOR_PAIR(COLOR_PAIR_YELLOW));
        wprintw(pad, "%s", item.c_str());
        y++;
    }
}

std::string class_to_title(const std::string& class_name, WINDOW* pad) {
    int color_pair = COLOR_PAIR_HEADER;
    std::string emoji = "❓";
    std::string title_text = "Dispositivo PCI";

    if (class_name.find("VGA") != std::string::npos || class_name.find("3D") != std::string::npos) {
        emoji = "🖥️"; title_text = "Placa de Vídeo"; color_pair = COLOR_PAIR_GREEN;
    } else if (class_name.find("Ethernet") != std::string::npos) {
        emoji = "🔌"; title_text = "Rede (Ethernet)"; color_pair = COLOR_PAIR_BLUE;
    } else if (class_name.find("Network") != std::string::npos) {
        emoji = "📡"; title_text = "Rede (Wireless)"; color_pair = COLOR_PAIR_BLUE;
    } else if (class_name.find("Audio") != std::string::npos) {
        emoji = "🎵"; title_text = "Áudio"; color_pair = COLOR_PAIR_MAGENTA;
    } else if (class_name.find("USB") != std::string::npos) {
        emoji = "🔗"; title_text = "USB"; color_pair = COLOR_PAIR_CYAN;
    } else if (class_name.find("SATA") != std::string::npos || class_name.find("Non-Volatile") != std::string::npos) {
        emoji = "📀"; title_text = "Armazenamento"; color_pair = COLOR_PAIR_YELLOW;
    } else if (class_name.find("Host bridge") != std::string::npos || class_name.find("PCI bridge") != std::string::npos) {
        emoji = "🧱"; title_text = "Ponte / Chipset"; color_pair = COLOR_PAIR_VALUE;
    }

    wattron(pad, COLOR_PAIR(color_pair));
    std::string full_title = emoji + " " + title_text;
    return full_title;
}

void print_centered_header(WINDOW* pad, int& y, const std::string& title) {
    int width = getmaxx(pad);
    int title_len = title.length() + 4; // for "  ...  "
    int padding = (width - title_len) / 2;

    wattron(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwhline(pad, y, 1, ACS_HLINE, width - 2);
    mvwprintw(pad, y + 1, padding, "  %s  ", title.c_str());
    mvwhline(pad, y + 2, 1, ACS_HLINE, width - 2);
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    y += 4;
}


// --- Main Application ---
int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); curs_set(0);
    start_color(); use_default_colors(); keypad(stdscr, TRUE);
    init_pair(COLOR_PAIR_HEADER, COLOR_CYAN, -1);
    init_pair(COLOR_PAIR_TITLE, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_VALUE, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_HINT, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_PAIR_GREEN, COLOR_GREEN, -1);
    init_pair(COLOR_PAIR_CYAN, COLOR_CYAN, -1);
    init_pair(COLOR_PAIR_YELLOW, COLOR_YELLOW, -1);
    init_pair(COLOR_PAIR_MAGENTA, COLOR_MAGENTA, -1);
    init_pair(COLOR_PAIR_BLUE, COLOR_BLUE, -1);
    init_pair(COLOR_PAIR_RED, COLOR_RED, -1);

    // --- Initial Data Fetch ---
    CpuInfo cpu = getCpuInfo();
    BoardInfo board = getBoardInfo();
    BiosInfo bios = getBiosInfo();
    std::vector<MonitorInfo> monitors = getMonitorInfo();
    std::vector<PciDevice> pci_devices = getAllPciDevices();

    // --- Pad Creation ---
    const int PAD_HEIGHT = 500; // A large virtual height for all content
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);
    WINDOW *pad = newpad(PAD_HEIGHT, term_cols);
    keypad(pad, TRUE);

    // --- Draw All Static Info to Pad ONCE ---
    int y = 1;

    print_centered_header(pad, y, "Relatório de Hardware do Sistema");

    print_kv(pad, y, "CPU Modelo", cpu.model);
    print_kv(pad, y, "CPU Núcleos", cpu.cores);
    print_kv(pad, y, "CPU Threads", cpu.threads);
    print_kv(pad, y, "CPU Frequência", cpu.max_freq);
    y++;

    print_kv(pad, y, "Placa-mãe Fab.", board.manufacturer);
    print_kv(pad, y, "Placa-mãe Prod.", board.product_name);
    y++;

    print_kv(pad, y, "BIOS Fab.", bios.vendor);
    print_kv(pad, y, "BIOS Versão", bios.version);
    print_kv(pad, y, "BIOS Data", bios.release_date);
    y++;

    if (!monitors.empty()) {
        for(const auto& mon : monitors) {
            print_kv(pad, y, "Monitor " + mon.name, mon.resolution);
        }
        y++;
    }

    print_centered_header(pad, y, "Dispositivos PCI");

    for (const auto& dev : pci_devices) {
        std::string title = class_to_title(dev.class_name, pad);
        wattron(pad, A_BOLD);
        mvwprintw(pad, y++, 1, title.c_str());
        wattroff(pad, A_BOLD);

        print_kv(pad, y, "Dispositivo", dev.name);
        print_kv(pad, y, "Fabricante", dev.vendor);
        print_kv(pad, y, "Slot PCI", dev.slot);
        print_kv(pad, y, "Driver", dev.driver, COLOR_PAIR_GREEN);
        print_kv(pad, y, "Módulos", dev.modules, COLOR_PAIR_CYAN);

        if (!dev.irq.empty()) {
             print_kv(pad, y, "IRQ", dev.irq);
        }
        if (!dev.regions.empty()) {
            print_bullets(pad, y, "Recursos", dev.regions);
        }
        for (const auto& attr : dev.additional_attributes) {
            print_kv(pad, y, attr.first, attr.second);
        }
        y++;
    }

    int total_lines = y;

    // --- Main Loop for Input and Refreshing ---
    int pad_pos = 0;
    int ram_y = 6; // Fixed Y position for RAM info
    while (true) {
        MemoryInfo mem = getMemoryInfo();
        wattron(pad, A_BOLD);
        mvwprintw(pad, ram_y -1, LABEL_WIDTH, "              "); // Clear previous value
        mvwprintw(pad, ram_y, LABEL_WIDTH, "              "); // Clear previous value
        print_kv(pad, ram_y, "Memória Total", mem.total);
        print_kv(pad, ram_y, "Memória em Uso", mem.used);
        wattroff(pad, A_BOLD);

        std::string hint = " Use as SETAS para rolar | Pressione 'q' para sair ";
        int hint_x = (term_cols - hint.length()) / 2;
        wattron(stdscr, COLOR_PAIR(COLOR_PAIR_HINT));
        mvwhline(stdscr, term_rows - 1, 0, ' ', term_cols);
        mvprintw(term_rows - 1, hint_x, hint.c_str());
        wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_HINT));

        prefresh(pad, pad_pos, 0, 0, 0, term_rows - 2, term_cols -1);

        int ch = wgetch(pad);
        if (ch == 'q' || ch == 'Q') break;

        switch(ch) {
            case KEY_DOWN:
                if (pad_pos < total_lines - term_rows + 5) pad_pos++;
                break;
            case KEY_UP:
                if (pad_pos > 0) pad_pos--;
                break;
            case KEY_RESIZE:
                getmaxyx(stdscr, term_rows, term_cols);
                clear();
                break;
        }
    }

    delwin(pad);
    endwin();
    return 0;
}