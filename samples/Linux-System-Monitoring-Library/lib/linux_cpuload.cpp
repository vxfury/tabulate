/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#include "linux_cpuload.hpp"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <chrono>
#include <thread>

const std::vector<std::string> cpuIdentifiers{"user",
                                        "nice",
                                        "system",
                                        "idle",
                                        "iowait",
                                        "irq",
                                        "softirq",
                                        "steal",
                                        "guest",
                                        "guest_nice"};

void cpuLoad::initCpuUsage() {
    this->oldCpuLoadMap = this->parseStatFile(this->procFile);
    std::this_thread::sleep_for(this->updateTime);
    this->cpuLoadMap = this->parseStatFile(this->procFile);
    this->calculateCpuUsage();
    this->timestamp_of_measurement = std::chrono::system_clock::now() - this->updateTime;
}

void cpuLoad::upDateCPUUsage() {
    if (( std::chrono::system_clock::now() - this->timestamp_of_measurement ) >=  this->updateTime) {
        this->oldCpuLoadMap = this->cpuLoadMap;
        this->timestamp_of_measurement = std::chrono::system_clock::now();
        this->cpuLoadMap = this->parseStatFile(this->procFile);
        this->calculateCpuUsage();
    }
}


double cpuLoad::getCurrentCpuUsage() {
    this->upDateCPUUsage();
    return this->cpuUsage.at("cpu");
}

std::vector<double> cpuLoad::getCurrentMultiCoreUsage() {
    this->upDateCPUUsage();
    std::vector<double> percents;
    for (const auto &elem: this->cpuUsage) {
        if (elem.first == "cpu") {
            continue;
        }
        percents.push_back(elem.second);
    }
    return percents;
}

void cpuLoad::calculateCpuUsage() {
    for (const auto &elem: this->cpuLoadMap) {

        if (this->cpuLoadMap.at(elem.first).at("user") > this->oldCpuLoadMap.at(elem.first).at("user") ||
            this->cpuLoadMap.at(elem.first).at("nice") > this->oldCpuLoadMap.at(elem.first).at("nice") ||
            this->cpuLoadMap.at(elem.first).at("system") > this->oldCpuLoadMap.at(elem.first).at("system") ||
            this->cpuLoadMap.at(elem.first).at("idle") > this->oldCpuLoadMap.at(elem.first).at("idle")) {
            auto total = (this->cpuLoadMap.at(elem.first).at("user") - this->oldCpuLoadMap.at(elem.first).at("user")) +
                         (this->cpuLoadMap.at(elem.first).at("nice") - this->oldCpuLoadMap.at(elem.first).at("nice")) +
                         (this->cpuLoadMap.at(elem.first).at("system") -
                          this->oldCpuLoadMap.at(elem.first).at("system"));

            double percent = total;
            total += (this->cpuLoadMap.at(elem.first).at("idle") - this->oldCpuLoadMap.at(elem.first).at("idle"));
            percent /= total;
            percent *= 100.0;
            this->cpuUsage[elem.first] = percent;
        }
    }
}

std::map<std::string, std::unordered_map<std::string, uint64_t>>  cpuLoad::parseStatFile(const std::string &fileName) {

    std::map<std::string, std::unordered_map<std::string, uint64_t>> cpuLoad_;

    try {
        std::ifstream cpuFile(fileName);

        uint32_t lineCnt = 0;
        bool infoValid = true;
        for (std::string line; std::getline(cpuFile, line) && infoValid; lineCnt++) {

            std::stringstream strStream(line);
            std::string strPart;
            std::string cpuNum;
            auto it = cpuIdentifiers.begin();
            std::unordered_map<std::string, uint64_t> cpuValues;
            while (strStream >> strPart) {
                if (cpuNum.empty()) {
                    if (strPart.find("cpu") != std::string::npos) {
                        cpuNum = strPart;
                        continue;
                    } else {
                        infoValid = false;
                        break;
                    }
                }
                if (it != cpuIdentifiers.end()) {
                    cpuValues[it->data()] = std::stoull(strPart);
                }
                if (it->data() == cpuIdentifiers.at(4)) {
                    break;
                }
                it++;
            }
            if (!cpuNum.empty()) {
                cpuLoad_[cpuNum] = cpuValues;
            }
        }
    } catch (std::ifstream::failure &e) {
        throw std::runtime_error("Exception: " + fileName + std::string(e.what()));
    }
    return cpuLoad_;
}

std::string cpuLoad::getCPUName(const std::string &cpuNameFile) {

    if (!this->cpuName.empty()) {
        return this->cpuName;
    }

    std::ifstream file;
    file.open(cpuNameFile);

    if (!file.is_open()) {
        throw std::runtime_error("unable to open " + cpuNameFile);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                this->cpuName = line.substr(pos, line.size());
                return this->cpuName;
            }
        }
    }
    return std::string();
}
