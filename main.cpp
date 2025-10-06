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

int print_info_in_win(WINDOW* win, int y, int x, const std::string& title, const std::string& value) {
    const int title_col = x, colon_col = x + 16, value_col = x + 18; const int value_max_width = getmaxx(win) - value_col - 1;
    wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE)); mvwprintw(win, y, title_col, title.c_str()); wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(win, y, colon_col, ":");
    std::vector<std::string> value_lines = wrap_text(value, value_max_width);
    wattron(win, COLOR_PAIR(COLOR_PAIR_VALUE)); int lines_used = value_lines.empty() ? 1 : 0;
    for (const auto& line : value_lines) { mvwprintw(win, y + lines_used, value_col, line.c_str()); lines_used++; }
    wattroff(win, COLOR_PAIR(COLOR_PAIR_VALUE)); return lines_used;
}
int calculate_content_height(const std::vector<std::string>& values, int value_max_width) {
    int total_height = 0; for (const auto& value : values) { total_height += wrap_text(value, value_max_width).size(); } return total_height;
}

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); curs_set(0); start_color(); use_default_colors(); keypad(stdscr, TRUE);
    init_pair(COLOR_PAIR_HEADER, COLOR_CYAN, -1); init_pair(COLOR_PAIR_TITLE, COLOR_YELLOW, -1); init_pair(COLOR_PAIR_VALUE, COLOR_WHITE, -1);

    int ch;
    while (true) {
        int term_rows, term_cols;
        getmaxyx(stdscr, term_rows, term_cols);

        if (term_cols < 80) {
            erase(); mvprintw(0, 0, "Terminal muito estreito. Largura mínima: 80 colunas.");
            refresh(); timeout(2000); ch = getch(); if (ch == 'q' || ch == 'Q') break; continue;
        }

        CpuInfo cpu = getCpuInfo(); MemoryInfo mem = getMemoryInfo(); BoardInfo board = getBoardInfo();
        BiosInfo bios = getBiosInfo(); std::vector<PciDevice> gpus = getGpuDevices();
        std::vector<MonitorInfo> monitors = getMonitorInfo(); std::vector<PciDevice> audio_devs = getAudioDevices();
        std::vector<PciDevice> net_devs = getNetworkDevices();

        int margin = 2; int full_width = term_cols - (margin * 2); int half_width = (full_width - margin) / 2;
        int right_half_width = full_width - half_width - margin; int right_half_start_x = margin + half_width + margin;

        int board_h = calculate_content_height({board.manufacturer, board.product_name, board.version, board.serial_number}, half_width - 20);
        int bios_h = calculate_content_height({bios.vendor, bios.version, bios.release_date}, right_half_width - 20);
        int board_bios_height = std::max(board_h, bios_h) + 2;
        int gpu_h = gpus.empty() ? 1 : calculate_content_height({gpus[0].vendor, gpus[0].name, gpus[0].driver}, half_width - 20);
        int mon_h = monitors.empty() ? 1 : 0;
        if (!monitors.empty()) { for(const auto& mon : monitors) { mon_h += calculate_content_height({mon.resolution}, right_half_width-20) + 1; } }
        int gpu_mon_height = std::max(gpu_h, mon_h) + 2;

        int audio_list_h = audio_devs.empty() ? 2 : (audio_devs.size() * 5) -1;
        int net_list_h = net_devs.empty() ? 2 : (net_devs.size() * 5) -1;

        int y_pos = 1;
        WINDOW *cpu_win = newwin(7, full_width, y_pos, margin); y_pos += 7 + 1;
        WINDOW *mem_win = newwin(4, full_width, y_pos, margin); y_pos += 4 + 1;
        WINDOW *board_win = newwin(board_bios_height, half_width, y_pos, margin);
        WINDOW *bios_win = newwin(board_bios_height, right_half_width, y_pos, right_half_start_x); y_pos += board_bios_height + 1;
        WINDOW *gpu_win = newwin(gpu_mon_height, half_width, y_pos, margin);
        WINDOW *mon_win = newwin(gpu_mon_height, right_half_width, y_pos, right_half_start_x); y_pos += gpu_mon_height + 1;
        WINDOW *audio_win = newwin(audio_list_h + 2, full_width, y_pos, margin); y_pos += audio_list_h + 2 + 1;
        WINDOW *net_win = newwin(net_list_h + 2, full_width, y_pos, margin);

        erase();

        int current_y;

        wattron(cpu_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(cpu_win, 0, 0); mvwprintw(cpu_win, 0, 2, " Processador (CPU) "); wattroff(cpu_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; current_y += print_info_in_win(cpu_win, current_y, 2, "Modelo", cpu.model); current_y += print_info_in_win(cpu_win, current_y, 2, "Núcleos", cpu.cores);
        current_y += print_info_in_win(cpu_win, current_y, 2, "Threads", cpu.threads); print_info_in_win(cpu_win, current_y, 2, "Frequência", cpu.max_freq);

        wattron(mem_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(mem_win, 0, 0); mvwprintw(mem_win, 0, 2, " Memória (RAM) "); wattroff(mem_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; current_y += print_info_in_win(mem_win, current_y, 2, "Total", mem.total); print_info_in_win(mem_win, current_y, 2, "Em Uso", mem.used);

        wattron(board_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(board_win, 0, 0); mvwprintw(board_win, 0, 2, " Placa-mãe "); wattroff(board_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; current_y += print_info_in_win(board_win, current_y, 2, "Fabricante", board.manufacturer); current_y += print_info_in_win(board_win, current_y, 2, "Produto", board.product_name);
        current_y += print_info_in_win(board_win, current_y, 2, "Versão", board.version); print_info_in_win(board_win, current_y, 2, "Nro. de Série", board.serial_number);

        wattron(bios_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(bios_win, 0, 0); mvwprintw(bios_win, 0, 2, " BIOS "); wattroff(bios_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; current_y += print_info_in_win(bios_win, current_y, 2, "Fabricante", bios.vendor);
        current_y += print_info_in_win(bios_win, current_y, 2, "Versão", bios.version); print_info_in_win(bios_win, current_y, 2, "Data", bios.release_date);

        wattron(gpu_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(gpu_win, 0, 0); mvwprintw(gpu_win, 0, 2, " Dispositivos Gráficos "); wattroff(gpu_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; if (!gpus.empty()) { current_y += print_info_in_win(gpu_win, current_y, 2, "Fabricante", gpus[0].vendor);
        current_y += print_info_in_win(gpu_win, current_y, 2, "Modelo", gpus[0].name); print_info_in_win(gpu_win, current_y, 2, "Driver em Uso", gpus[0].driver); } else { mvwprintw(gpu_win, 1, 2, "Nenhum dispositivo encontrado."); }

        wattron(mon_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(mon_win, 0, 0); mvwprintw(mon_win, 0, 2, " Monitores "); wattroff(mon_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; if (!monitors.empty()) { for(const auto& mon : monitors) { current_y += print_info_in_win(mon_win, current_y, 2, mon.name, mon.resolution); } } else { mvwprintw(mon_win, 1, 2, "Nenhum monitor conectado."); }

        wattron(audio_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(audio_win, 0, 0); mvwprintw(audio_win, 0, 2, " Dispositivos de Som "); wattroff(audio_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; if (!audio_devs.empty()) { for (size_t i = 0; i < audio_devs.size(); ++i) {
            current_y += print_info_in_win(audio_win, current_y, 2, "Nome", audio_devs[i].name); current_y += print_info_in_win(audio_win, current_y, 2, "Fabricante", audio_devs[i].vendor);
            current_y += print_info_in_win(audio_win, current_y, 2, "Driver", audio_devs[i].driver);
            if (i < audio_devs.size() - 1) { std::string separator(getmaxx(audio_win) > 2 ? getmaxx(audio_win) - 2 : 0, '-');
                wattron(audio_win, COLOR_PAIR(COLOR_PAIR_HEADER)); mvwprintw(audio_win, current_y++, 1, separator.c_str()); wattroff(audio_win, COLOR_PAIR(COLOR_PAIR_HEADER)); }
        } } else { mvwprintw(audio_win, 1, 2, "Nenhum dispositivo encontrado."); }

        wattron(net_win, COLOR_PAIR(COLOR_PAIR_HEADER)); box(net_win, 0, 0); mvwprintw(net_win, 0, 2, " Dispositivos de Rede "); wattroff(net_win, COLOR_PAIR(COLOR_PAIR_HEADER));
        current_y = 1; if (!net_devs.empty()) { for (size_t i = 0; i < net_devs.size(); ++i) {
            current_y += print_info_in_win(net_win, current_y, 2, "Nome", net_devs[i].name); current_y += print_info_in_win(net_win, current_y, 2, "Fabricante", net_devs[i].vendor);
            current_y += print_info_in_win(net_win, current_y, 2, "Driver", net_devs[i].driver);
            if (i < net_devs.size() - 1) { std::string separator(getmaxx(net_win) > 2 ? getmaxx(net_win) - 2 : 0, '-');
                wattron(net_win, COLOR_PAIR(COLOR_PAIR_HEADER)); mvwprintw(net_win, current_y++, 1, separator.c_str()); wattroff(net_win, COLOR_PAIR(COLOR_PAIR_HEADER)); }
        } } else { mvwprintw(net_win, 1, 2, "Nenhum dispositivo encontrado."); }

        wnoutrefresh(stdscr); wnoutrefresh(cpu_win); wnoutrefresh(mem_win); wnoutrefresh(board_win);
        wnoutrefresh(bios_win); wnoutrefresh(gpu_win); wnoutrefresh(mon_win); wnoutrefresh(audio_win); wnoutrefresh(net_win);
        doupdate();

        delwin(cpu_win); delwin(mem_win); delwin(board_win); delwin(bios_win);
        delwin(gpu_win); delwin(mon_win); delwin(audio_win); delwin(net_win);

        timeout(2000); ch = getch(); if (ch == 'q' || ch == 'Q') break; if (ch == KEY_RESIZE) erase();
    }

    endwin();
    return 0;
}