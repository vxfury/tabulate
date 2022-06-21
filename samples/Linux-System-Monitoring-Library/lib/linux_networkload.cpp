/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#include "linux_networkload.hpp"
#include <string>
#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <utility>

const std::list<std::string> identifiers{"RXbytes",
                                             "RXpackets",
                                             "RXerrs",
                                             "RXdrop",
                                             "RXfifo",
                                             "RXframe",
                                             "RXcompressed",
                                             "RXmulticast",
                                             "TXbytes",
                                             "TXpackets",
                                             "TXerrs",
                                             "TXdrop",
                                             "TXfifo",
                                             "TXcolls",
                                             "TXcarrier",
                                             "TXcompressed"
};

std::string networkLoad::mapEnumToString(networkLoad::networkParam param) {
    auto it = identifiers.begin();
    std::advance(it,static_cast<uint32_t>(param));
    return it->data();
}


std::shared_ptr<networkLoad::networkParser> networkLoad::networkParser::inst = nullptr;

networkLoad::networkLoad(std::string ethernetDataFileName, std::string ethName) : ethernetDataFile(std::move(ethernetDataFileName)), ethDev(std::move(ethName)) {
    networkParser::getNetworkParser()->parse(ethernetDataFileName);
}

void networkLoad::networkParser::parse(const std::string& netFile) {

    if((this->currentTime + std::chrono::milliseconds(1000)) > std::chrono::system_clock::now()) {
        return;
    } else {
        this->timeBefore = this->currentTime;
        this->currentTime = std::chrono::system_clock::now();
    }

    std::ifstream ethFile;

    try {
        ethFile.open(netFile);
    } catch (std::ifstream::failure &e) {
        throw std::runtime_error("Exception: "+ netFile + std::string(e.what()));
    }

    this->timeBefore = std::chrono::system_clock::now();
    this->ethObjOld = this->ethObj;

    uint32_t lineCnt = 0;
    for(std::string line; std::getline(ethFile, line); lineCnt++) {
        if(lineCnt < 2) {
            continue;
        }
        std::stringstream strStream(line);
        std::string strPart;
        std::string ifName = "";
        std::unordered_map<std::string, uint64_t> ifValues;
        auto it = identifiers.begin();
        while(strStream >> strPart) {
            if(ifName.empty()) {
                strPart.erase(std::remove_if(strPart.begin(),strPart.end(),[](auto c) {
                    return !std::isalnum(c);
                }), strPart.end());
                ifName = strPart;
            } else {
                if(it != identifiers.end()) {
                    ifValues[it->data()] = std::stoull(strPart);
                }
                it++;
            }
        }
        this->ethObj[ifName] = ifValues;
    }
    if(this->ethObjOld.empty()) {
        this->ethObjOld = this->ethObj;
    }
}

const std::unordered_map<std::string, uint64_t>
&networkLoad::networkParser::getEthObj(const std::string& ethDevice) const {
    return this->ethObj.at(ethDevice);
}

const std::unordered_map<std::string, uint64_t> &
networkLoad::networkParser::getEthObjOld(const std::string& ethDevice) const {
    return this->ethObjOld.at(ethDevice);
}


std::list<std::string> networkLoad::networkParser::getNetworkDevices(std::string netFile) {
    std::list<std::string> ifList;
    this->parse(netFile);
    for(const auto& elem: this->ethObj) {
        ifList.push_back(elem.first);
    }
    return ifList;
}

networkLoad::networkParser::networkParser() {
    this->currentTime = std::chrono::system_clock::now() - std::chrono::milliseconds (2000);
    this->timeBefore = std::chrono::system_clock::now();
}

std::shared_ptr<networkLoad::networkParser> networkLoad::networkParser::getNetworkParser() {
    if(networkLoad::networkParser::inst == nullptr) {
        networkLoad::networkParser::inst = std::make_unique<networkLoad::networkParser>();
    }
    return networkLoad::networkParser::inst;
}

const std::chrono::system_clock::time_point networkLoad::networkParser::getTimeStamp() const {
    return this->currentTime;
}



const std::chrono::system_clock::time_point networkLoad::networkParser::getTimeBefore() const {
    return this->timeBefore;
}


bool networkLoad::isDeviceUp() const {
    return this->isDeviceAvailable;
}


std::string networkLoad::getDeviceName() {
    return this->ethDev;
}

std::list<std::string> networkLoad::scanNetworkDevices(const std::string& ethernetDataFile) {

    auto parser = networkParser::getNetworkParser();
    return parser->getNetworkDevices(ethernetDataFile);
}

std::string networkLoad::getBytesPerSeceondString(uint64_t bytesPerSecond) {
    return getBytesString(bytesPerSecond) + "/s";
}

std::string networkLoad::getBitsPerSeceondString(uint64_t bytesPerSecond) {
    return getBitsString(bytesPerSecond) + "/s";
}

std::string networkLoad::getBytesString(uint64_t totalBytes) {
    uint64_t Bytes = totalBytes;
    uint64_t kByte = 0;
    uint64_t mByte = 0;
    uint64_t gByte = 0;
    if (Bytes > 1024) {
        kByte = Bytes / 1024;
        Bytes %= 1024;
    }
    if (kByte > 1024) {
        mByte = kByte / 1024;
        kByte %= 1024;
    }
    if(mByte > 1024) {
        gByte = mByte / 1024;
        mByte %=1024;
    }

    if (gByte > 0) {
        std::string net;
        net += std::to_string(gByte);
        net += ".";
        net += std::to_string(mByte / 100);
        net += "gByte";
        return net;
    }


    if (mByte > 0) {
        std::string net;
        net += std::to_string(mByte);
        net += ".";
        net += std::to_string(kByte / 100);
        net += "mByte";
        return net;
    }

    if (kByte > 0) {
        std::string net;
        net += std::to_string(kByte);
        net += ".";
        net += std::to_string(Bytes / 100);
        net += "kByte";
        return net;
    }

    std::string net;
    net += std::to_string(Bytes);
    net += "Byte";
    return net;
}

std::string networkLoad::getBitsString(uint64_t totalBytes) {
    uint64_t Bytes = totalBytes * 8;
    uint64_t kByte = 0;
    uint64_t mByte = 0;
    uint64_t gByte = 0;
    if (Bytes > 1024) {
        kByte = Bytes / 1024;
        Bytes %= 1024;
    }
    if (kByte > 1024) {
        mByte = kByte / 1024;
        kByte %= 1024;
    }
    if(mByte > 1024) {
        gByte = mByte / 1024;
        mByte %=1024;
    }

    if (gByte > 0) {
        std::string net;
        net += std::to_string(gByte);
        net += ".";
        net += std::to_string(mByte / 100);
        net += "gBit";
        return net;
    }


    if (mByte > 0) {
        std::string net;
        net += std::to_string(mByte);
        net += ".";
        net += std::to_string(kByte / 100);
        net += "mBit";
        return net;
    }

    if (kByte > 0) {
        std::string net;
        net += std::to_string(kByte);
        net += ".";
        net += std::to_string(Bytes / 100);
        net += "kBit";
        return net;
    }


    std::string net;
    net += std::to_string(Bytes);
    net += "Bit";
    return net;
}

uint64_t networkLoad::getParamPerSecond(std::string designator) {
    networkParser::getNetworkParser()->parse(this->ethernetDataFile);
    if(!std::count_if(identifiers.begin(),identifiers.end(),[designator](auto elem) {
        return elem == designator;
    })) {
        throw std::runtime_error("invalid designator: " + designator);
    }
    auto before = networkParser::getNetworkParser()->getTimeBefore();
    auto current = networkParser::getNetworkParser()->getTimeStamp();

    auto msec = std::chrono::duration_cast<std::chrono::milliseconds> (
            before - current);

    uint64_t Bytes = (networkParser::getNetworkParser()->getEthObj(this->ethDev).at(designator) -
            networkParser::getNetworkParser()->getEthObjOld(this->ethDev).at(designator));
    if (static_cast<unsigned long>(msec.count()) <= 0) {
        Bytes /= 1;
    } else {
        Bytes /= static_cast<unsigned long>(msec.count());
    }
    return Bytes;
}

uint64_t networkLoad::getParamSinceStartup(std::string designator) {
    networkParser::getNetworkParser()->parse(this->ethernetDataFile);
    auto ifObj = networkParser::getNetworkParser()->getEthObj(this->ethDev);
    if(!std::count_if(identifiers.begin(),identifiers.end(),[designator](auto elem) {
        return elem == designator;
    })) {
        throw std::runtime_error("invalid designator: " + designator);
    }
    return ifObj[designator];
}

