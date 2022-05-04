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
    timer() : starttime(clock::now()) {}

    void reset()
    {
        starttime = clock::now();
    }

    void pause()
    {
        pausetime = clock::now();
    }

    void resume()
    {
        starttime += clock::now() - pausetime;
    }

    template <typename type = double>
    type elasped()
    {
        return std::chrono::duration<type>(clock::now() - starttime).count();
    }

  private:
    std::chrono::time_point<clock> starttime;
    std::chrono::time_point<clock> pausetime;
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
        printf("Time Elapsed: %.2f s.\n", tmr.elasped());
    }

    return 0;
}
