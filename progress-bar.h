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

#pragma once

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <unordered_map>

/* CSI sequence: https://www.liquisearch.com/ansi_escape_code/csi_codes */

std::string replace_all(std::string str, const std::string &from, const std::string &to)
{
    size_t curr = 0;
    while ((curr = str.find(from, curr)) != std::string::npos) {
        str.replace(curr, from.length(), to);
        curr += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

#define TRACE(fmt, ...) // printf("%s:%d" fmt "\n", __func__, __LINE__, #__VA_ARGS__)

class ProgressBar {
  public:
    struct ProgressData {
        time_t starttime;
        size_t max_steps;
        std::atomic<double> percentage;
        std::unordered_map<std::string, std::string> args;

        ProgressData(size_t max_steps) : starttime(time(nullptr)), max_steps(max_steps), percentage(0.0) {}

        double add(double progress)
        {
            return percentage = std::min(100.0, percentage + progress);
        }

        double set(double progress)
        {
            return percentage = std::max(0.0, std::min(100.0, progress));
        }

        double advance(size_t steps)
        {
            return add(100.0 * steps / static_cast<double>(max_steps));
        }

        std::string format(const std::string &format, size_t bar_width)
        {
            // auto filled pairs
            {
                auto format_time = [](time_t t) -> std::string {
                    char buf[32], *p = buf;
                    int m = t / 60;
                    int s = t % 60;
                    if (m >= 60) {
                        m %= 60;
                        p += snprintf(p, 32 - (buf - p), "%d:", m / 60);
                    }
                    snprintf(p, 32 - (buf - p), "%02d:%02d", m, s);

                    return std::string(buf);
                };

                auto format_bar = [](double p, size_t width) -> std::string {
                    size_t l = static_cast<size_t>(p * width / 100.0 + 0.5);
                    return std::string(l, '>') + std::string(width - l, ' ');
                };

                auto format_progress = [](double p) -> std::string {
                    char buff[32];
                    snprintf(buff, sizeof(buff), "%5.1f", p);
                    return std::string(buff);
                };

                time_t now = time(nullptr);
                args["progress"] = format_progress(percentage);
                args["bar"] = format_bar(percentage, bar_width);
                args["elapsed"] = format_time(now - starttime);
                if (percentage >= 1e-3) {
                    args["remaining"] = format_time((now - starttime) * (100.0 - percentage) / percentage);
                } else {
                    args["remaining"] = "--:--";
                }
            }

            std::string formated;
            size_t n = 0;
            while (n < format.size()) {
                size_t k = format.find('{', n);
                size_t e = format.find('}', k);
                if (k == std::string::npos || e == std::string::npos) {
                    formated += format.substr(n);
                    break;
                }
                formated += format.substr(n, k - n);
                size_t f = format.find(':', k + 1);
                if (f != std::string::npos && f < e) {
                    std::string fmt = format.substr(k + 1, f - k - 1);
                    std::string key = format.substr(f + 1, e - f - 1);
                    if (args.count(key)) {
                        char buff[fmt.size() + args.at(key).size()];
                        snprintf(buff, sizeof(buff), fmt.c_str(), args.at(key).c_str());
                        formated += std::string(buff);
                    }
                } else {
                    std::string key = format.substr(k + 1, e - k - 1);
                    if (args.count(key)) {
                        formated += args.at(key);
                    }
                }
                n = e + 1;
            }

            return formated;
        }
    };

    struct ProgressWidget {
        int fd;
        bool leave;
        bool disable;
        size_t max_width;
        size_t bar_width;
        std::string format;
        bool keep_cursor_hidden;
        std::tuple<int, int> position;

        ProgressWidget(bool disable) : disable(disable) {}
        ProgressWidget(std::string format = "{progress} {elapsed} | {bar} | {remaining}",
                       std::tuple<int, int> position = ProgressWidget::getpos(), int fd = STDOUT_FILENO,
                       bool leave = true, bool disable = false, size_t max_width = 0, size_t bar_width = 0,
                       bool keep_cursor_hidden = false)
            : fd(fd),
              leave(leave),
              disable(disable),
              max_width(max_width),
              bar_width(bar_width),
              format(std::move(format)),
              keep_cursor_hidden(keep_cursor_hidden),
              position(std::move(position))
        {
            if (isatty(fd)) {
                struct winsize w;
                ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                if (max_width == 0) {
                    this->max_width = w.ws_col - 8;
                }
            }
            if (bar_width == 0) {
                this->bar_width = this->max_width * 0.5;
            }
        }
        ~ProgressWidget()
        {
            if (!disable) {
                std::string outputs;
                if (std::get<0>(position) > 0) {
                    outputs += leave ? "" : "\x1B[s";
                    outputs += "\x1B[" + std::to_string(std::get<0>(position)) + ";"
                               + std::to_string(std::get<1>(position)) + "H";
                }
                outputs += leave ? "\x1B[E" : "\x1B[2K";
                outputs += keep_cursor_hidden ? "" : "\x1B[?25h";
                outputs += (!leave && std::get<0>(position) > 0) ? "\x1B[u" : "";
                write(fd, outputs.c_str(), outputs.size());
                TRACE("%s", replace_all(outputs, "\x1B", "ESC ").c_str());
            }
        }

        void clear()
        {
            if (!disable) {
                std::string outputs;
                if (std::get<0>(position) > 0) {
                    outputs += "\x1B[s\x1B[" + std::to_string(std::get<0>(position)) + ";"
                               + std::to_string(std::get<1>(position)) + "H";
                }
                outputs += leave ? "\x1B[E" : "\x1B[2K";
                outputs += keep_cursor_hidden ? "" : "\x1B[?25h";
                if (std::get<0>(position) > 0) {
                    outputs += "\x1B[u";
                }
                write(fd, outputs.c_str(), outputs.size());
                TRACE("%s", replace_all(outputs, "\x1B", "ESC ").c_str());
            }
        }

        void refresh(std::shared_ptr<ProgressData> data)
        {
            if (disable) {
                return;
            }

            std::string outputs = "\x1B[?25l";
            if (std::get<0>(position) > 0) {
                outputs += "\x1B[s\x1B[" + std::to_string(std::get<0>(position)) + ";"
                           + std::to_string(std::get<1>(position)) + "H";
            } else {
                outputs += "\r";
            }
            outputs += "\x1B[2K";
            auto line = data->format(format, bar_width);
            TRACE("%s", replace_all(line, "\x1B", " ESC ").c_str());
            if (max_width != 0 && line.length() > max_width) {
                std::string truncated = "...";
                if (max_width >= truncated.length()) {
                    line = line.substr(0, max_width - truncated.length()) + truncated + "\x1B[0m";
                } else {
                    line = line.substr(0, max_width) + "\x1B[0m";
                }
            }
            outputs += line;
            if (std::get<0>(position) > 0) {
                outputs += "\x1B[u";
            }
            write(fd, outputs.c_str(), outputs.size());
            TRACE("%s", replace_all(outputs, "\x1B", "ESC ").c_str());
        }

        static std::tuple<int, int> getpos()
        {
            int fd = -1;
            {
                const char *dev = ttyname(STDIN_FILENO);
                if (dev == nullptr) {
                    errno = ENOTTY;
                    return std::tuple<int, int>(-1, -1);
                }
                fd = open(dev, O_RDWR | O_NOCTTY, 0);
            }

            struct termios ts, ots;
            tcgetattr(fd, &ts);
            memcpy(&ots, &ts, sizeof(struct termios));
            ts.c_lflag &= ~(ECHO | ICANON | CREAD);
            tcsetattr(fd, TCSADRAIN, &ts);

            char buf[16];
            {
                /* How many characters in the input queue. */
                int m = 0;
#ifdef TIOCINQ
                ioctl(fd, TIOCINQ, &m);
#elif defined(FIONREAD)
                ioctl(fd, FIONREAD, &m);
#endif

                /* Read them all. */
                char discarded[m];
                m = read(fd, discarded, m);

                write(fd, "\x1B[6n", sizeof("\x1B[6n"));

                int n = read(fd, buf, 19);
                buf[n] = '\0';

                ts.c_lflag |= ICANON;
                tcsetattr(fd, TCSADRAIN, &ts);
                for (int i = 0; i < m; i++) {
                    ioctl(fd, TIOCSTI, discarded + i);
                }
            }

            int x, y;
            tcsetattr(fd, TCSADRAIN, &ots);
            if (sscanf(buf, "\033[%d;%dR", &x, &y) == 2) {
                return std::tuple<int, int>(x, y);
            }

            return std::tuple<int, int>(-1, -1);
        }
    };

    ProgressBar(bool disable) : widget(std::make_shared<ProgressWidget>(disable)) {}
    ProgressBar(std::shared_ptr<ProgressWidget> widget, std::shared_ptr<ProgressData> data) : data(data), widget(widget)
    {
    }
    ProgressBar(std::shared_ptr<ProgressWidget> widget, size_t max_steps = 100,
                std::shared_ptr<ProgressBar> overall_bar = ProgressBar::disabled_bar())
        : data(std::make_shared<ProgressData>(max_steps)), widget(widget), overall_bar(overall_bar)
    {
    }

    ProgressBar(std::string format = "{progress} {elapsed} | {bar} | {remaining}",
                std::tuple<int, int> position = ProgressWidget::getpos(), int fd = STDOUT_FILENO, bool leave = true,
                bool disable = false, size_t max_steps = 100, size_t max_width = 0, size_t bar_width = 0,
                bool keep_cursor_hidden = false, std::shared_ptr<ProgressBar> overall_bar = ProgressBar::disabled_bar())
        : data(std::make_shared<ProgressData>(max_steps)),
          widget(std::make_shared<ProgressWidget>(format, std::move(position), fd, leave, disable, max_width, bar_width,
                                                  keep_cursor_hidden)),
          overall_bar(overall_bar)
    {
    }

    double advance(size_t steps)
    {
        if (data.get()) {
            std::lock_guard<std::mutex> guard(lock);
            if (data->percentage != data->advance(steps) && widget.get()) {
                widget->refresh(data);
            }
            return data->percentage;
        } else {
            return 0.0;
        }
    }

    double set_progress(double progress)
    {
        if (data.get()) {
            std::lock_guard<std::mutex> guard(lock);
            if (data->percentage != data->set(progress) && widget.get()) {
                widget->refresh(data);
            }
            return data->percentage;
        } else {
            return 0.0;
        }
    }

    void add_arg(std::string key, std::string value)
    {
        if (data.get()) {
            data->args[key] = value;
        }
    }

    void add_args(std::initializer_list<std::pair<std::string, std::string>> _args)
    {
        if (data.get()) {
            for (auto arg : _args) {
                data->args[arg.first] = arg.second;
            }
        }
    }

    void set_args(std::initializer_list<std::pair<std::string, std::string>> _args)
    {
        std::lock_guard<std::mutex> guard(lock);
        if (data.get()) {
            data->args = std::unordered_map<std::string, std::string>(_args.begin(), _args.end());
        }
    }

    ProgressBar &overall()
    {
        return overall_bar.get() ? *overall_bar : *ProgressBar::disabled_bar();
    }

    static std::shared_ptr<ProgressBar> disabled_bar()
    {
        static std::shared_ptr<ProgressBar> __ = std::make_shared<ProgressBar>(true);
        return __;
    }

    bool disabled()
    {
        return widget.get() ? widget->disable : true;
    }

  private:
    std::mutex lock;
    std::shared_ptr<ProgressData> data;
    std::shared_ptr<ProgressWidget> widget;
    std::shared_ptr<ProgressBar> overall_bar;
};

class ProgressBars {
  public:
    ProgressBars(std::string format, size_t size, size_t max_width = 0, size_t bar_width = 0, bool leave = false,
                 bool disable = false, std::tuple<int, int> position = ProgressBar::ProgressWidget::getpos(),
                 size_t overall_steps = 0)
        : leave(leave), disable(disable), position(std::move(position))
    {
        std::tuple<int, int> pos = position;
        if (overall_steps != 0) {
            overall_bar = std::make_shared<ProgressBar>("{progress} {elapsed} | {bar} | {remaining}", position,
                                                        STDOUT_FILENO, true, disable, overall_steps, max_width,
                                                        bar_width, false, ProgressBar::disabled_bar());
            std::get<0>(pos) += std::get<0>(position) > 0 ? 1 : 0;
        }

        for (size_t i = 0; i < size; i++) {
            widgets.push_back(std::make_shared<ProgressBar::ProgressWidget>(format, pos, STDOUT_FILENO, leave, disable,
                                                                            max_width, bar_width, true));
            std::get<0>(pos) += std::get<0>(position) > 0 ? 1 : 0;
        }
    }
    ~ProgressBars()
    {
        size_t off = 0;
        if (overall_bar.get()) {
            off += 1;
            overall_bar.reset(); // destory before widgets
        }
        if (leave) {
            off += widgets.size();
            routine_bars.clear();
            widgets.clear();
        } else {
            for (auto widget : widgets) {
                widget->clear();
            }
        }
        if (!disable) {
            std::string outputs;
            if (std::get<0>(position) > 0) {
                outputs += "\x1B[" + std::to_string(std::get<0>(position) + off) + ";"
                           + std::to_string(std::get<1>(position)) + "H\x1B[2K";
            }
            outputs += "\x1B[?25h";
            write(STDOUT_FILENO, outputs.c_str(), outputs.size());
            TRACE("%s", replace_all(outputs, "\x1B", "ESC ").c_str());
        }
    }

    ProgressBar &operator[](size_t index)
    {
        if (index >= routine_bars.size()) {
            assert(widgets.size() > 0);
            routine_bars.push_back(std::make_shared<ProgressBar>(widgets[index % widgets.size()], 100, overall_bar));
        }
        assert(routine_bars.size() > 0);
        return *routine_bars[index % routine_bars.size()];
    }

  private:
    bool leave;
    bool disable;
    std::tuple<int, int> position;
    std::shared_ptr<ProgressBar> overall_bar;
    std::vector<std::shared_ptr<ProgressBar>> routine_bars;
    std::vector<std::shared_ptr<ProgressBar::ProgressWidget>> widgets;
};
