#include "profiler.h"

#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <chrono>

#define HIT_FREQEUENCY(n, N, T)                       \
    ({                                                \
        static time_t reset_time = 0;                 \
        static unsigned int count = 0;                \
        const unsigned int tmp = N > n ? (N / n) : 1; \
        auto time_optimized = []() {                  \
            struct timeval tv;                        \
            struct timezone tz;                       \
            gettimeofday(&tv, &tz);                   \
            return tv.tv_sec;                         \
        };                                            \
        if (reset_time < time_optimized() - T) {      \
            count = 0; /* at least one time each T */ \
            reset_time = time_optimized();            \
        }                                             \
        (count++) % tmp == 0;                         \
    })

int main(int argc, char **argv)
{
    size_t repeat = 10000;
    if (argc >= 2) {
        repeat = atoi(argv[1]);
    }

    Profiler::SetTitle("Benchmark of Time APIs");

    Profiler::Add(
        "time",
        []() {
            DoNotOptimize(time(nullptr));
            return true;
        },
        repeat);

    Profiler::Add(
        "clock_gettime",
        []() {
            struct timespec ts;
            DoNotOptimize(clock_gettime(CLOCK_MONOTONIC, &ts));
            return true;
        },
        repeat);

    Profiler::Add(
        "clock_gettime-seconds",
        []() {
            DoNotOptimize([]() {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                return ts.tv_sec;
            });
            return true;
        },
        repeat);

    Profiler::Add(
        "gettimeofday",
        []() {
            struct timeval tv;
            DoNotOptimize(gettimeofday(&tv, nullptr));
            return true;
        },
        repeat);

    Profiler::Add(
        "gettimeofday-seconds",
        []() {
            DoNotOptimize([]() {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                return tv.tv_sec;
            });
            return true;
        },
        repeat);

    Profiler::Add(
        "localtime",
        []() {
            time_t t = time(nullptr);
            DoNotOptimize(localtime(&t));
            return true;
        },
        repeat);

    Profiler::Add(
        "localtime_r",
        []() {
            time_t t = time(nullptr);
            struct tm tm;
            DoNotOptimize(localtime_r(&t, &tm));
            return true;
        },
        repeat);

    Profiler::Add(
        "chrono::high_resolution_clock",
        []() {
            DoNotOptimize(std::chrono::high_resolution_clock::now());
            return true;
        },
        repeat);

    Profiler::Add(
        "HIT_FREQEUENCY",
        []() {
            DoNotOptimize(HIT_FREQEUENCY(10, 10000, 1));
            return true;
        },
        repeat);

    return 0;
}
