/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#pragma once

#include <string>
#include <chrono>

class memoryLoad {
public:
    explicit memoryLoad(std::string memInfo = "/proc/meminfo",
                        std::string memInfoOfProcess = "/proc/self/status",
                        std::string memInfoOfProcessPrefix = "/proc/self/"):
                        totalMemoryInKB(0),
                        currentMemoryUsageInKB(0),
                        memInfoFile(std::move(memInfo)),
                        memInfoOfProcessFile(std::move(memInfoOfProcess)),
                        memInfoOfProcessPrefixFile(std::move(memInfoOfProcessPrefix))
                        {};
    /**
     * @brief get total memory of the system in KB
     * @return total memory in KB
     */
    uint64_t getTotalMemoryInKB();
    /**
     * @brief get current Memory Usage of the system in KB
     * @return used memory in KB
     */
    uint64_t getCurrentMemUsageInKB();
    /**
     * @brief get current memory usage of the system in percent 0-100%
     * @return 0-100%
     */
    float getCurrentMemUsageInPercent();
    /**
     * @brief get current memory usage of given memory usage in percent 0-100%
     * @return 0-100%
     */
    static float calcMemoryUsageInPercent(uint64_t currentUsedMemory, uint64_t totalMemory);
    /**
     * @brief get the current memory usage of a process
     * @param pid  - process id
     * @return memory usage in KB
     */
    static uint64_t getMemoryUsedByProcess(int pid);
    /**
     * @brief get memory usage of this process (self)
     * @return memory usage in KB
     */
    uint64_t getMemoryUsageByThisProcess();

private:
    bool parseMemoryFile();
    static uint64_t parseProcessMemoryFile(std::string fileToParse);
    uint64_t totalMemoryInKB;
    uint64_t currentMemoryUsageInKB;
    std::string memInfoFile;
    std::string memInfoOfProcessFile;
    std::string memInfoOfProcessPrefixFile;
    std::chrono::time_point<std::chrono::steady_clock> timeStamp;
};