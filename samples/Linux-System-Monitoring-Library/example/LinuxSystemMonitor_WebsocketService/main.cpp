/**
 * @author: Daniel Fuchs
 * @contact: fuxeysolutions@gmail.com
 *
 * distributed under the MIT License (MIT).
 * Copyright (c) Daniel Fuchs
 *
 */
/*
  _ _                                _                                       _
 | (_)                              | |                                     (_)
 | |_ _ __  _   ___  _____ _   _ ___| |_ ___ _ __ ___  _ __ ___   ___  _ __  _  ___  _ __
 | | | '_ \| | | \ \/ / __| | | / __| __/ _ \ '_ ` _ \| '_ ` _ \ / _ \| '_ \| |/ _ \| '__|
 | | | | | | |_| |>  <\__ \ |_| \__ \ ||  __/ | | | | | | | | | | (_) | | | | | (_) | |
 |_|_|_| |_|\__,_/_/\_\___/\__, |___/\__\___|_| |_| |_|_| |_| |_|\___/|_| |_|_|\___/|_|
                            __/ |
                           |___/

 */



#include <memory>
#include <iostream>
#include <json.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#include <cxxopts.hpp>
#include "linuxsysmonitor.hpp"
#include <utility>

#include <App.h>


using json = nlohmann::json;


class PerSocketData : public IObserver {
public:
    PerSocketData() {
        this->connectedWSUs = nullptr;
        this->connectedWS = nullptr;
    }

    void
    shutdown() {
        this->linuxMon->Detach(this);
    }

    void
    setConnectedWebsocket(uWS::WebSocket<false, true> *socket,
                          std::shared_ptr<linuxsysmonitor> linuxSysMonitorInstance) {
        this->connectedWSUs = socket;
        this->linuxMon = std::move(linuxSysMonitorInstance);
        this->linuxMon->Attach(this);
        std::cout << "send to ws " << this->connectedWSUs << std::endl;
        this->connectedWSUs->send("send json dump", uWS::OpCode::TEXT);
    }

    void
    setConnectedWebsocket(uWS::WebSocket<true, true> *socket,
                          std::shared_ptr<linuxsysmonitor> linuxSysMonitorInstance) {
        this->connectedWS = socket;
        this->linuxMon = std::move(linuxSysMonitorInstance);
        this->linuxMon->Attach(this);
        std::cout << "send to ws " << this->connectedWS << std::endl;
        this->connectedWS->send("send json dump", uWS::OpCode::TEXT);
    }

    void
    Update(const json &message_from_subject) override {
        if(this->connectedWS != nullptr) {
            this->connectedWS->send(message_from_subject.dump(), uWS::OpCode::TEXT);
        }
        if(this->connectedWSUs != nullptr) {
            this->connectedWSUs->send(message_from_subject.dump(), uWS::OpCode::TEXT);
        }
    }

private:
    uWS::WebSocket<true, true> *connectedWS;
    uWS::WebSocket<false, true> *connectedWSUs;
    std::shared_ptr<linuxsysmonitor> linuxMon;
};

class LinuxMonitorWebSocketBridge {

private:
    uint16_t portToListen;
    std::string publicCerts;
    std::string privateKey;
    std::string passPhrase;
    std::unique_ptr<std::thread> spawnThread;
    uWS::TemplatedApp<true> inst;
    std::shared_ptr<linuxsysmonitor> linuxMon;


    void startServer(std::string route) {
        if (!this->privateKey.empty() && !this->publicCerts.empty()) {

            uWS::SSLApp({
                                .key_file_name = this->privateKey.c_str(),
                                .cert_file_name = this->publicCerts.c_str(),
                                .passphrase = this->passPhrase.c_str()
                        }).ws<PerSocketData>(route, {
                    .compression = uWS::SHARED_COMPRESSOR,
                    .maxPayloadLength = 16 * 1024,
                    .idleTimeout = 100,
                    .maxBackpressure = 1 * 1024 * 1024,
                    .upgrade = [](auto *res, auto *req, auto *context) {
                        std::cout << "upgrade to wss" << std::endl;
                    },
                    .open = [&](auto *ws) {
                        std::cout << "open ws adr: " << ws << std::endl;
                        static_cast<PerSocketData *>(ws->getUserData())->setConnectedWebsocket(ws, this->linuxMon);
                    },
                    .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                        std::cout << "message ws adr: " << ws << std::endl;
                        ws->send(message, opCode, true);
                    },
                    .drain = [](auto *ws) {
                        std::cout << "drain ws adr: " << ws << std::endl;
                    },
                    .ping = [](auto *ws) {
                        std::cout << "ping ws adr: " << ws << std::endl;
                    },
                    .pong = [](auto *ws) {
                        std::cout << "pong ws adr: " << ws << std::endl;
                    },
                    .close = [](auto *ws, int code, std::string_view message) {
                        std::cout << "close ws adr:" << ws << std::endl;
                        static_cast<PerSocketData *>(ws->getUserData())->shutdown();
                    }
            }).listen(this->portToListen, [&](us_listen_socket_t *token) {
                if (token) {
                    std::cout << "Secure WebsocketServer Listening on port "
                              << this->portToListen
                              << " with route "
                              << route << std::endl;
                }
            }).run();
        } else {
            uWS::App().ws<PerSocketData>(route, {
                    .compression = uWS::SHARED_COMPRESSOR,
                    .maxPayloadLength = 16 * 1024,
                    .idleTimeout = 100,
                    .maxBackpressure = 1 * 1024 * 1024,
                    .open = [&](auto *ws) {
                        std::cout << "open ws adr: " << ws << std::endl;
                        static_cast<PerSocketData *>(ws->getUserData())->setConnectedWebsocket(ws, this->linuxMon);

                    },
                    .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                        std::cout << "message ws adr: " << ws << std::endl;
                        ws->send(message, opCode, true);
                    },
                    .drain = [](auto *ws) {
                        std::cout << "drain ws adr: " << ws << std::endl;
                    },
                    .ping = [](auto *ws) {
                        std::cout << "ping ws adr: " << ws << std::endl;
                    },
                    .pong = [](auto *ws) {
                        std::cout << "pong ws adr: " << ws << std::endl;
                    },
                    .close = [](auto *ws, int code, std::string_view message) {
                        std::cout << "close ws adr:" << ws << std::endl;
                        static_cast<PerSocketData *>(ws->getUserData())->shutdown();
                    }
            }).listen(this->portToListen, [&](us_listen_socket_t *token) {
                if (token) {
                    std::cout << "Unsecure WebsocketServer Listening on port "
                              << this->portToListen
                              << " with route "
                              << route << std::endl;
                }
            }).run();
        }
        std::cerr << "undefined behaviour" << std::endl;
    }


public:

    explicit LinuxMonitorWebSocketBridge(const std::string& privateKeyPath,
                                         const std::string& publicCertPath,
                                         const std::string& passphrase = "",
                                         const uint16_t portNum = 4002,
                                         const std::chrono::milliseconds &timeoutInterval = std::chrono::milliseconds(
                                                 1000),
                                         const std::string& defaultprocPath = "/proc/") {

        std::cout << "Create LinuxMonitorWebsocketBridge instance" << this << std::endl;
        if (!publicCertPath.empty() and !privateKeyPath.empty()) {
            std::cout << "use SSL " << publicCertPath << " : " << privateKeyPath << std::endl;
        }
        this->portToListen = portNum;
        this->publicCerts = publicCertPath;
        this->privateKey = privateKeyPath;
        this->passPhrase = passphrase;

        linuxMon = std::make_shared<linuxsysmonitor>(timeoutInterval, defaultprocPath);
    }

    void runServer(std::string route, bool detach = false) {
        spawnThread = std::make_unique<std::thread>([&]() { this->startServer(route); });
        if (detach) {
            spawnThread->detach();
        } else {
            spawnThread->join();
        }
    }

};


int main(int argc, const char *argv[]) {

    cxxopts::Options options("Lightweight Linux Kernel Load Monitoring",
                             "get precises information about CPU, Memory and Network load!");
    options.add_options()
            ("d,debug", "Enable debugging")
            ("c,public_cert", "path to public cert", cxxopts::value<std::string>()->default_value(""))
            ("k,private_key", "path to private key", cxxopts::value<std::string>()->default_value(""))
            ("p,websocketport_num", "Port to listen on", cxxopts::value<int>()->default_value("4004"))
            ("i,sendInterval", "sendInterval where data sent out in milliseconds",
             cxxopts::value<int>()->default_value("1000"))
            ("f,json", "File name", cxxopts::value<std::string>())
            ("b,abspath", "Path to access kernel informationm",
             cxxopts::value<std::string>()->default_value("/proc/"));
    auto result = options.parse(argc, argv);

    auto webSocketLinuxMonitor = std::make_unique<LinuxMonitorWebSocketBridge>(
            std::string(result["private_key"].as<std::string>()),
            std::string(result["public_cert"].as<std::string>()),
            "",
            result["websocketport_num"].as<int>(),
            std::chrono::milliseconds(result["sendInterval"].as<int>()),
            std::string(result["abspath"].as<std::string>())
    );

    webSocketLinuxMonitor->runServer("/linuxmonitor", false);
    return 0;
}

