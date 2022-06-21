/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#pragma once



#include <tuple>
#include <map>
#include <unordered_map>
#include <string>

class linuxProcessLoad {

public:
    /**
     * @brief get a map of [pid] which contains the cpu load between two calls.
     *          function needs to be called regularly (e.g.: 5s.) to get the cpu load per process
     * @return const map[pid][cpuload]
     */
    std::map<std::string, double> getProcessCpuLoad();
    /**
      * @brief get a map of [pid] which contains the cpu load between two calls.
      *          function needs to be called regularly (e.g.: 5s.) to get the cpu load per process
      * @return const map[pid][cpuload]
      */
    std::map<int, double> getPidCpuLoad();


private:
    void parseProcess(const std::string& pid);
    void findProcesses();
    void calculateProcessLoad();
    std::map<std::string, double> procCPUUsage;
    std::map<int, double> pidCPUUsage;
    std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> oldCpuTimes;
    std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> CpuTimes;
    std::map<std::string,std::unordered_map<std::string, std::string>> processStat;
    std::map<std::string,std::unordered_map<std::string, std::string>> oldProcessStat;
};


