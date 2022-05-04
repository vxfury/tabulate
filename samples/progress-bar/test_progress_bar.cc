/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <sstream>
#include <iostream>
#include "threadpool.h"
#include "progress-bar.h"

#include <stdio.h>

int getch_noblocking(void)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    return select(1, &rfds, NULL, NULL, &tv) >= 1 ? getchar() : -1;
}

template <typename clock = std::chrono::high_resolution_clock>
class timer {
  public:
    timer()
    {
        reset();
    }

    void reset()
    {
        starttime = clock::now();
        get_process_details(&utime_last, &stime_last, &sutime_last, &cstime_last, &num_threads_last);
    }

    std::string description() const
    {
        unsigned long utime, stime;
        long int sutime, cstime, num_threads;
        if (get_process_details(&utime, &stime, &sutime, &cstime, &num_threads) == 0) {
            utime -= utime_last;
            stime -= stime_last;
            sutime -= sutime_last;
            cstime -= cstime_last;
            auto format_time = [](double t) -> std::string {
                std::stringstream ss;
                ss.precision(3);
                ss << t;
                return ss.str();
            };
            double sysclk = static_cast<double>(sysconf(_SC_CLK_TCK));
            return "real: " + format_time(std::chrono::duration<double>(clock::now() - starttime).count())
                   + "s, user mode: " + format_time(utime / sysclk) + " s, kernel mode: " + format_time(stime / sysclk)
                   + " s, chirdren(user mode): " + format_time(sutime / sysclk)
                   + " s, chirdren(kernel mode): " + format_time(cstime / sysclk) + " s, threads: ("
                   + std::to_string(num_threads) + ", " + std::to_string(num_threads_last) + ")";
        }

        return "<Not avaliable>";
    }

  private:
    unsigned long utime_last, stime_last;
    long int sutime_last, cstime_last, num_threads_last;
    std::chrono::time_point<std::chrono::high_resolution_clock> starttime;

    static int get_process_details(unsigned long *putime, unsigned long *pstime, long int *psutime, long int *pcstime,
                                   long int *pnthreads)
    {
        int fd;
        char filename[256];
        sprintf(filename, "/proc/%i/stat", getpid());
        if ((fd = open(filename, O_RDONLY)) >= 0) {
            int size;
            char proc_stat[512];
            if ((size = read(fd, proc_stat, sizeof(proc_stat))) > 0) {
                const char *spcifiers =
                    "%*d %*s %*c %*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %lu %lu %ld %ld %*ld %*ld %ld";
                sscanf(proc_stat, spcifiers,
                       putime,   // (14)  Amount of time that this process has been scheduled in user mode, mea‐
                                 //   sured in clock ticks (divide by sysconf(_SC_CLK_TCK)).  This includes  guest
                                 //   time,  guest_time  (time  spent  running  a virtual CPU, see below), so that
                                 //   applications that are not aware of the guest time field  do  not  lose  that
                                 //   time from their calculations.
                       pstime,   // (15)  Amount  of  time  that this process has been scheduled in kernel mode,
                                 // measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
                       psutime,  // (16) Amount of time that this process's waited-for children have been sched‐
                                 //   uled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
                                 //   (See also times(2).)  This includes guest time, cguest_time (time spent run‐
                                 //   ning a virtual CPU, see below).
                       pcstime,  // (17) Amount of time that this process's waited-for children have been sched‐
                                 //   uled   in   kernel   mode,   measured   in   clock    ticks    (divide    by
                                 //   sysconf(_SC_CLK_TCK)).
                       pnthreads // (20) Number of threads in this process (since  Linux  2.6).   Before  kernel
                                 //   2.6,  this field was hard coded to 0 as a placeholder for an earlier removed
                                 //   field.
                );
            }

            close(fd);
        } else {
            return -errno;
        }

        return 0;
    }
};


int main(int argc, char **argv)
{
    srand(time(nullptr));

    bool pause_enabled = false;
    if (argc >= 2 && strcmp(argv[1], "--pause") == 0) {
        pause_enabled = true;
    }

    {
        ProgressBar bar;
        for (int i = 0; i < 100; i++) {
            bar.advance(1);
            usleep(rand() % 20000);
        }
    }

    {
        timer tmr;
        {
            size_t size = 6, max_steps = 10000;

            ProgressBars bars("{progress} {elapsed} | {bar} | {remaining} {index}...", size, 0, 0, true, false,
                              ProgressBar::ProgressWidget::getpos(), max_steps);
            {
                auto progress_bar_test = [](int i, size_t length, ProgressBar &bar) {
                    size_t remain = length;
                    bar.add_arg("index", std::to_string(i));
                    while (remain > 0) {
                        bar.set_progress(100.0 * (length - remain) / static_cast<double>(length));
                        remain -= rand() % (remain + 1);
                        usleep(rand() % 1000);
                    }
                    bar.overall().advance(1);
                };

                multiprocessing::threadpool<> pool(18);
                for (size_t i = 0; i < max_steps; i++) {
                    auto &bar = bars[i];
                    pool.push([&, i]() {
                        progress_bar_test(i, rand() % 9001 + 1000, bar);
                    });
                }
                if (pause_enabled) { // for debugging or testing
                    int ch;
                    bool paused = false, autopaused = false;
                    while ((ch = getch_noblocking()) != 'q') {
                        if (ch == 'p') {
                            if (!paused) {
                                pool.pause();
                            } else {
                                pool.resume();
                            }
                            paused = !paused;
                        } else if (ch == 'a') {
                            autopaused = true;
                        } else if (autopaused) {
                            auto pos = ProgressBar::ProgressWidget::getpos();
                            if (std::get<1>(pos) >= 110) {
                                pool.pause();
                                paused = true;
                            }
                        }
                    }
                }
                pool.wait();
            }
        }
        std::cout << tmr.description() << std::endl;
    }

    return 0;
}
