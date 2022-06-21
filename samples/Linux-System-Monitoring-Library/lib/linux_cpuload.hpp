/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <map>
#include <unordered_map>
#include <chrono>

class cpuLoad {

public:
    cpuLoad() = delete;
    /**
     * @brief constructor
     * @param procFileName
     * @param updateTime - std::chrono obj
     */
        explicit cpuLoad(std::string procFileName = "/proc/stat", std::chrono::milliseconds updateTime_ = std::chrono::milliseconds(1000)):
        procFile(std::move(procFileName)), updateTime(updateTime_), cpuName("") {};
    /**
     * @brief initialize the parsing algo
     */
    void initCpuUsage();
    /**
     * @brief get current cpu load in 0-100%
     * @return current cpu load
     */
    double getCurrentCpuUsage();

    /**
     * @brief get Cpu user / nice / system /idle time. used for cpu usage per process
     * @return tuple<user,nice,system,idle>
     */
    std::tuple<uint64_t , uint64_t, uint64_t, uint64_t> getCpuTimes() {
        auto cpuLoad_ = this->parseStatFile(this->procFile);
        return std::make_tuple(cpuLoad_.at("cpu").at("user"),
                               cpuLoad_.at("cpu").at("nice"),
                               cpuLoad_.at("cpu").at("system"),
                               cpuLoad_.at("cpu").at("idle"));
    }

    /**
     * @brief get cpu Usage of all cores in percent
     * @return vector of cpu load per core 0-100%
     */
    std::vector<double> getCurrentMultiCoreUsage();
    /**
     * @brief get CPU Description
     * @param cpuNameFile - typical /proc/cpuinfo
     * @return cpu Type string
     */
    std::string getCPUName(const std::string& cpuNameFile = "/proc/cpuinfo");

private:
    void calculateCpuUsage();
    std::map<std::string, std::unordered_map<std::string, uint64_t>> parseStatFile(const std::string& fileName);
    void upDateCPUUsage();
    std::chrono::system_clock::time_point timestamp_of_measurement;
    std::string procFile;
    std::chrono::milliseconds updateTime;
    std::string cpuName;
    std::map<std::string, double> cpuUsage;
    std::map<std::string, std::unordered_map<std::string, uint64_t>> cpuLoadMap;
    std::map<std::string, std::unordered_map<std::string, uint64_t>> oldCpuLoadMap;
};


