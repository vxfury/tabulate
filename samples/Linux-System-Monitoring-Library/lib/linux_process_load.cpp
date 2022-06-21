/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */

#include "linux_process_load.hpp"
#include <list>
#include <memory>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "linux_cpuload.hpp"
#include "linux_memoryload.hpp"

static const std::list<std::string> stats {"pid",
                                    "comm",
                                    "state",
                                    "ppid",
                                    "pgrp",
                                    "session",
                                    "tty_nr",
                                    "tpgid",
                                    "flags",
                                    "minflt",
                                    "cminflt",
                                    "majflt",
                                    "cmajflt",
                                    "utime",
                                    "stime",
                                    "cutime",
                                    "cstime",
                                    "priority",
                                    "nice",
                                    "num_threads",
                                    "itrealvalue",
                                    "starttime",
                                    "vsize",
                                    "rss",
                                    "rsslim",
                                    "startcode",
                                    "endcode",
                                    "startstack",
                                    "kstkesp",
                                    "kstkeip",
                                    "signal",
                                    "blocked",
                                    "sigignore",
                                    "sigcatch",
                                    "wchan",
                                    "nswap",
                                    "cnswap",
                                    "exit_signal",
                                    "processor",
                                    "rt_priority",
                                    "policy",
                                    "delaycct_blkio_ticks",
                                    "guest_time",
                                    "cguest_time",
                                    "start_data",
                                    "end_data",
                                    "start_brk",
                                    "arg_start",
                                    "arg_end",
                                    "env_start",
                                    "env_end",
                                    "exit_code"};

std::map<std::string, double> linuxProcessLoad::getProcessCpuLoad() {
    this->findProcesses();
    return this->procCPUUsage;
}

std::map<int, double> linuxProcessLoad::getPidCpuLoad() {
    this->findProcesses();
    return this->pidCPUUsage;
}



void linuxProcessLoad::calculateProcessLoad() {
    auto [ cpuTotalUserTime, cpuTotalUserLowTime, cpuTotalSysTime, cpuTotalIdleTime] = CpuTimes;

    auto [ oldCpuTotalUserTime, oldCpuTotalUserLowTime, oldCpuTotalSysTime, oldCpuTotalIdleTime] = oldCpuTimes;
    auto TotalUserTime = cpuTotalUserTime - oldCpuTotalUserTime;
    auto TotalSysTime = cpuTotalSysTime - oldCpuTotalSysTime;

    this->procCPUUsage.clear();
    for(const auto& elem: processStat) {
        auto pid = elem.first;
        try {
            auto oldProc = oldProcessStat.at(pid);
            auto proc = elem.second;
            auto procName = proc.at("comm");
            uint64_t cpuTime = 0;
            cpuTime += (std::stoull(proc.at("utime")) - std::stoull(oldProc.at("utime")));
            cpuTime += (std::stoull(proc.at("stime")) - std::stoull(oldProc.at("stime")));


            double percentage = ((static_cast<double>(cpuTime) *100.0) / static_cast<double>((TotalUserTime + TotalSysTime)) );

            if(percentage > 0.1) {
                this->procCPUUsage[procName] = percentage;
            }
            this->pidCPUUsage[std::stoi(pid)] = percentage;

        } catch(...) {
            std::cerr << "process: " << pid  << " disappeared in meantime" << std::endl;
        }
    }
}


void linuxProcessLoad::findProcesses() {

    auto cpuMonitoring = std::make_unique<cpuLoad>("/proc/stat");

    this->CpuTimes = cpuMonitoring->getCpuTimes();


    std::string path{"/proc/"};
    std::vector<std::string> processes;
    for(auto& elem: std::filesystem::directory_iterator(path)) {
        auto procPath = elem.path().string();
        if(elem.exists()) {
            procPath.erase(procPath.begin(), procPath.begin() + static_cast<int32_t >(path.size()));
            if (std::isdigit(procPath.at(0))) {
                if (!std::count_if(procPath.begin(), procPath.end(), [](auto c) {
                    return std::isalpha(c);
                })) {
                    parseProcess(procPath);
                }
            }
        }
    }
    this->calculateProcessLoad();

    this->oldProcessStat = this->processStat;
    this->oldCpuTimes = this->CpuTimes;
}

void linuxProcessLoad::parseProcess(const std::string& pid) {
    std::string path {"/proc/" + pid + "/stat"};
    std::ifstream ethFile(path);

    std::string strPart;
    std::unordered_map<std::string, std::string> procStat;
    auto identifierStart = stats.begin();
    enum state {
        normal,
        isProcessParse
    };
    bool isProcessFound = false;
    while (ethFile >> strPart) {

        if(identifierStart != stats.end()) {
            if(isProcessFound) {
                procStat[identifierStart->data()] += " " + strPart;
            }
            if(std::count_if(strPart.begin(),strPart.end(),[](auto c) { return c == '('; })) {
                isProcessFound = true;
                procStat[identifierStart->data()] = strPart;
            }
            if(!isProcessFound) {
                procStat[identifierStart->data()] = strPart;
            }


            if(std::count_if(strPart.begin(), strPart.end(), [] (auto c) { return c == ')';})) {
                isProcessFound = false;
            }

        }
        if(!isProcessFound) {
            identifierStart++;
            continue;
        }

    }

    procStat["comm"].erase(std::remove_if(procStat["comm"].begin(),procStat["comm"].end(), [](auto c) {
        return c == '(' || c == ')';
    }), procStat["comm"].end());
    processStat[pid] = procStat;
}

