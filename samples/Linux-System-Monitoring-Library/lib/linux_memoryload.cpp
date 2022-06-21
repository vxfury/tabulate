/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#include "linux_memoryload.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <cinttypes>
bool memoryLoad::parseMemoryFile() {
    if(timeStamp + std::chrono::milliseconds(100) > std::chrono::steady_clock::now()) {
        return true;
    }
    std::ifstream memoryFile;
    memoryFile.open(this->memInfoFile);
    this->timeStamp = std::chrono::steady_clock::now();
    if (!memoryFile.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(memoryFile, line)) {
        sscanf(line.c_str(), "MemTotal: %" PRIu64, &this->totalMemoryInKB);
        sscanf(line.c_str(), "MemAvailable: %" PRIu64, &this->currentMemoryUsageInKB);
    }
    memoryFile.close();
    return true;
}

uint64_t memoryLoad::getTotalMemoryInKB() {
    this->parseMemoryFile();
    return this->totalMemoryInKB;
}

uint64_t memoryLoad::getCurrentMemUsageInKB() {
    this->parseMemoryFile();
    return this->getTotalMemoryInKB() - this->currentMemoryUsageInKB;
}

float memoryLoad::calcMemoryUsageInPercent(uint64_t currentUsedMemory, uint64_t totalMemory) {
    return round((((currentUsedMemory * 100.0 / totalMemory))) * 100.0) / 100.0;
}


float memoryLoad::getCurrentMemUsageInPercent() {
    this->parseMemoryFile();
    uint64_t memavail = this->getCurrentMemUsageInKB();
    return round((((memavail * 100.0 / this->getTotalMemoryInKB()))) * 100.0) / 100.0;
}

uint64_t memoryLoad::getMemoryUsageByThisProcess() {
    return this->parseProcessMemoryFile(this->memInfoOfProcessFile);
}

uint64_t memoryLoad::getMemoryUsedByProcess(int pid) {
    return memoryLoad::parseProcessMemoryFile("/proc/" + std::to_string(pid) + "/status");
}

uint64_t memoryLoad::parseProcessMemoryFile(std::string fileToParse) {
    uint64_t MemFree = 0;
    std::ifstream memoryFile;
    memoryFile.open(fileToParse);
    std::string line;
    while (std::getline(memoryFile, line)) {
        sscanf(line.c_str(), "VmSize: %" PRIu64, &MemFree);
    }
    return MemFree;
}
