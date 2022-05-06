#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <unistd.h>
#include <sys/select.h>

#include "tabulate.h"
using namespace tabulate;

int getch_noblocking(void)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    return select(1, &rfds, NULL, NULL, &tv) >= 1 ? getchar() : -1;
}

int main()
{
    std::cout << "\x1b[s";
    while (true) {
        Table process_table;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);

        process_table.add("PID", "%CPU", "%MEM", "User", "NI");
        process_table.add("4297", round(dis(gen) * 100), round(dis(gen) * 100), "ubuntu", "20");
        process_table.add("12671", round(dis(gen) * 100), round(dis(gen) * 100), "root", "0");
        process_table.add("810", round(dis(gen) * 100), round(dis(gen) * 100), "root", "-20");

        process_table.column(2).format().align(Align::center);
        process_table.column(3).format().align(Align::right);
        process_table.column(4).format().align(Align::right);

        for (size_t i = 0; i < 5; ++i) {
            process_table[0][i].format().color(Color::yellow).align(Align::center).styles(Style::bold);
        }

        std::cout << "\x1b[u";
        std::cout << process_table.xterm() << std::endl;
        std::cout << "\nPress ENTER to exit..." << std::endl;

        if (getch_noblocking() == '\n') {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
