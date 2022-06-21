/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
#pragma once


#include <map>
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <unordered_map>


class networkLoad {

public:
    static std::list<std::string> scanNetworkDevices(const std::string& ethernetDataFile= "/proc/net/dev");
    static std::vector<std::shared_ptr<networkLoad>> createLinuxEthernetScanList(const std::string& ethernetDataFileName = "/proc/net/dev") {
        std::vector<std::shared_ptr<networkLoad>> v;
        for (const auto& elem: networkLoad::scanNetworkDevices(ethernetDataFileName)) {
            v.push_back(std::make_shared<networkLoad>(ethernetDataFileName,elem));
        }
        return v;
    }

    explicit networkLoad(std::string ethernetDataFileName = "/proc/net/dev", std::string ethName = "eth0");
    uint64_t getParamPerSecond(std::string designator);
    uint64_t getParamSinceStartup(std::string designator);

    static std::string getBytesPerSeceondString(uint64_t bytesPerSecond);
    static std::string getBitsPerSeceondString(uint64_t bytesPerSecond);
    static std::string getBytesString(uint64_t totalBytes);
    static std::string getBitsString(uint64_t totalBytes);

    bool isDeviceUp() const;
    std::string getDeviceName();



    enum networkParam {
        RXbytes = 0,
        RXpackets,
        RXerrs,
        RXdrop,
        RXfifo,
        RXframe,
        RXcompressed,
        RXmulticast,
        TXbytes,
        TXpackets,
        TXerrs,
        TXdrop,
        TXfifo,
        TXcolls,
        TXcarrier,
        TXcompressed
    };
    static std::string mapEnumToString(networkLoad::networkParam param);
private:

    std::string ethernetDataFile;
    std::string ethDev;
    bool isDeviceAvailable = false;


    class networkParser {
    private:
        std::chrono::system_clock::time_point currentTime;
        std::chrono::system_clock::time_point timeBefore;

        std::map<std::string, std::unordered_map<std::string, uint64_t>> ethObj;
        std::map<std::string, std::unordered_map<std::string, uint64_t>> ethObjOld;

    public:
        static std::shared_ptr<networkParser> getNetworkParser();
        networkParser();
        void parse(const std::string& netFile = "/proc/net/dev");
        const std::unordered_map<std::string, uint64_t> &getEthObj(const std::string& ethDevice) const;
        std::list<std::string> getNetworkDevices(std::string netFile = "/proc/net/dev");
        const std::chrono::system_clock::time_point getTimeStamp() const;
        const std::unordered_map<std::string, uint64_t> &getEthObjOld(const std::string& ethDevice) const;
        const std::chrono::system_clock::time_point getTimeBefore() const;

        static std::shared_ptr<networkParser> inst;
    };


};

