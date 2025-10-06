#include <ncurses.h>
#include <string>
#include <locale.h>
#include <vector>
#include <sstream>
#include <algorithm>

#include "sys_info.hpp"

#define COLOR_PAIR_HEADER 1
#define COLOR_PAIR_TITLE  2
#define COLOR_PAIR_VALUE  3
#define COLOR_PAIR_HINT   4

// --- Helper Functions for Drawing ---

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
    return lines;
}

// Prints a key-value pair and returns the number of lines used
int print_info(WINDOW* pad, int& y, const std::string& title, const std::string& value, int value_col_width) {
    const int title_col = 2, colon_col = 18, value_col = 20;
    wattron(pad, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(pad, y, title_col, title.c_str());
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(pad, y, colon_col, ":");

    std::vector<std::string> value_lines = wrap_text(value, value_col_width);
    int lines_used = 0;
    wattron(pad, COLOR_PAIR(COLOR_PAIR_VALUE));
    for (const auto& line : value_lines) {
        mvwprintw(pad, y + lines_used, value_col, line.c_str());
        lines_used++;
    }
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_VALUE));
    y += lines_used;
    return lines_used;
}

// Prints a section header with a box
void print_header(WINDOW* pad, int& y, int width, const std::string& title) {
    wattron(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    box(pad, 0, 0); // Temporary box for width calculation, will be overwritten
    mvwhline(pad, y, 1, ACS_HLINE, width - 2);
    mvwprintw(pad, y, 3, " %s ", title.c_str());
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    y += 2;
}

// Prints a visual separator
void print_separator(WINDOW* pad, int& y, int width) {
    wattron(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    mvwhline(pad, y, 1, ACS_HLINE, width - 2);
    wattroff(pad, COLOR_PAIR(COLOR_PAIR_HEADER));
    y++;
}

// --- Main Application ---
int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); curs_set(0);
    start_color(); use_default_colors(); keypad(stdscr, TRUE);
    init_pair(COLOR_PAIR_HEADER, COLOR_CYAN, -1);
    init_pair(COLOR_PAIR_TITLE, COLOR_YELLOW, -1);
    init_pair(COLOR_PAIR_VALUE, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_HINT, COLOR_BLACK, COLOR_WHITE);

    // --- Initial Data Fetch ---
    CpuInfo cpu = getCpuInfo();
    BoardInfo board = getBoardInfo();
    BiosInfo bios = getBiosInfo();
    std::vector<PciDevice> gpus = getGpuDevices();
    std::vector<MonitorInfo> monitors = getMonitorInfo();
    std::vector<PciDevice> audio_devs = getAudioDevices();
    std::vector<PciDevice> net_devs = getNetworkDevices();

    // --- Pad Creation ---
    const int PAD_HEIGHT = 200; // A large virtual height for all content
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);
    WINDOW *pad = newpad(PAD_HEIGHT, term_cols);
    keypad(pad, TRUE);

    // --- Draw All Static Info to Pad ONCE ---
    int y = 1;
    int content_width = term_cols - 4;
    int value_width = content_width - 20;

    print_header(pad, y, term_cols, "Processador (CPU)");
    print_info(pad, y, "Modelo", cpu.model, value_width);
    print_info(pad, y, "Núcleos", cpu.cores, value_width);
    print_info(pad, y, "Threads", cpu.threads, value_width);
    print_info(pad, y, "Frequência", cpu.max_freq, value_width);
    y++;

    print_header(pad, y, term_cols, "Placa-mãe & BIOS");
    print_info(pad, y, "Fabricante", board.manufacturer, value_width);
    print_info(pad, y, "Produto", board.product_name, value_width);
    print_info(pad, y, "Versão", board.version, value_width);
    print_info(pad, y, "Nro. de Série", board.serial_number, value_width);
    print_separator(pad, y, term_cols);
    print_info(pad, y, "BIOS Fabricante", bios.vendor, value_width);
    print_info(pad, y, "BIOS Versão", bios.version, value_width);
    print_info(pad, y, "BIOS Data", bios.release_date, value_width);
    y++;

    print_header(pad, y, term_cols, "Dispositivos Gráficos");
    if (!gpus.empty()) {
        for (const auto& gpu : gpus) {
            print_info(pad, y, "Fabricante", gpu.vendor, value_width);
            print_info(pad, y, "Modelo", gpu.name, value_width);
            print_info(pad, y, "Driver em Uso", gpu.driver, value_width);
            print_separator(pad, y, term_cols);
        }
    } else {
        mvwprintw(pad, y++, 2, "Nenhum dispositivo encontrado.");
    }
    y++;

    print_header(pad, y, term_cols, "Monitores");
    if (!monitors.empty()) {
        for (const auto& mon : monitors) {
            print_info(pad, y, mon.name, mon.resolution, value_width);
        }
    } else {
        mvwprintw(pad, y++, 2, "Nenhum monitor conectado.");
    }
    y++;

    print_header(pad, y, term_cols, "Dispositivos de Som");
    if(!audio_devs.empty()) {
        for (const auto& dev : audio_devs) {
            print_info(pad, y, "Fabricante", dev.vendor, value_width);
            print_info(pad, y, "Modelo", dev.name, value_width);
            print_info(pad, y, "Driver", dev.driver, value_width);
            print_separator(pad, y, term_cols);
        }
    } else {
         mvwprintw(pad, y++, 2, "Nenhum dispositivo encontrado.");
    }
    y++;

    print_header(pad, y, term_cols, "Dispositivos de Rede");
    if(!net_devs.empty()) {
        for (const auto& dev : net_devs) {
            print_info(pad, y, "Fabricante", dev.vendor, value_width);
            print_info(pad, y, "Modelo", dev.name, value_width);
            print_info(pad, y, "Driver", dev.driver, value_width);
            print_separator(pad, y, term_cols);
        }
    } else {
         mvwprintw(pad, y++, 2, "Nenhum dispositivo encontrado.");
    }
    y++;

    int total_lines = y;

    // --- Main Loop for Input and Refreshing ---
    int pad_pos = 0;
    int ram_y = 6; // Fixed Y position for RAM info
    while (true) {
        // Fetch and display dynamic info (RAM)
        MemoryInfo mem = getMemoryInfo();
        print_header(pad, ram_y, term_cols, "Memória (RAM)");
        int temp_y = ram_y; // use temp to not affect main y
        print_info(pad, temp_y, "Total", mem.total, value_width);
        print_info(pad, temp_y, "Em Uso", mem.used, value_width);

        // Display scroll hint
        std::string hint = " Use as SETAS para rolar | Pressione 'q' para sair ";
        int hint_x = (term_cols - hint.length()) / 2;
        wattron(stdscr, COLOR_PAIR(COLOR_PAIR_HINT));
        mvwhline(stdscr, term_rows - 1, 0, ' ', term_cols);
        mvprintw(term_rows - 1, hint_x, hint.c_str());
        wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_HINT));

        // Refresh the screen with the visible part of the pad
        prefresh(pad, pad_pos, 0, 0, 0, term_rows - 2, term_cols -1);

        int ch = wgetch(pad);
        if (ch == 'q' || ch == 'Q') break;

        switch(ch) {
            case KEY_DOWN:
                if (pad_pos < total_lines - term_rows + 2) {
                    pad_pos++;
                }
                break;
            case KEY_UP:
                if (pad_pos > 0) {
                    pad_pos--;
                }
                break;
            case KEY_RESIZE:
                getmaxyx(stdscr, term_rows, term_cols);
                // On resize, clear the screen to prevent artifacts from the terminal's own redraw.
                // The main loop will then handle redrawing the pad and hint bar correctly.
                clear();
                break;
        }
    }

    // --- Cleanup ---
    delwin(pad);
    endwin();
    return 0;
}