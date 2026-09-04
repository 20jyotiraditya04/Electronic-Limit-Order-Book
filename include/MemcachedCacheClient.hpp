//
// Created by martian_duck on 25/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_MEMCACHEDCACHECLIENT_HPP
#define ELECTRONIC_LIMIT_ORDER_BOOK_MEMCACHEDCACHECLIENT_HPP

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_MEMCACHEDCACHECLIENT_HPP


#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ICacheClient.hpp"

class MemcachedCacheClient : public ICacheClient {
private:
    std::string host_;
    int port_;
    int sock_fd_{-1};

public:
    MemcachedCacheClient(std::string host = "127.0.0.1", int port = 11211)
        : host_(std::move(host)), port_(port) {}

    ~MemcachedCacheClient() override {
        disconnect();
    }

    bool connect_to_server() noexcept override {
        sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd_ < 0) return false;

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_);
        if (inet_pton(AF_INET, host_.c_str(), &serv_addr.sin_addr) <= 0) {
            disconnect();
            return false;
        }

        if (connect(sock_fd_, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
            disconnect();
            return false;
        }
        return true;
    }

    void disconnect() noexcept override {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
            sock_fd_ = -1;
        }
    }

    bool set(std::string_view key, std::string_view value) noexcept override {
        if (sock_fd_ < 0) return false;

        // Memcached ASCII command: set <key> <flags> <exptime> <bytes>\r\n<value>\r\n
        std::string req = "set " + std::string(key) + " 0 0 " + std::to_string(value.size()) + "\r\n" +
                          std::string(value) + "\r\n";

        if (write(sock_fd_, req.data(), req.size()) < 0) return false;

        char buf[64];
        ssize_t n = read(sock_fd_, buf, sizeof(buf) - 1);
        if (n <= 0) return false;
        buf[n] = '\0';
        return (strncmp(buf, "STORED", 6) == 0);
    }

    std::optional<std::string> get(std::string_view key) noexcept override {
        if (sock_fd_ < 0) return std::nullopt;

        // Memcached ASCII command: get <key>\r\n
        std::string req = "get " + std::string(key) + "\r\n";
        if (write(sock_fd_, req.data(), req.size()) < 0) return std::nullopt;

        char buf[1024];
        ssize_t n = read(sock_fd_, buf, sizeof(buf) - 1);
        if (n <= 0) return std::nullopt;
        buf[n] = '\0';

        // Response format: VALUE <key> <flags> <bytes>\r\n<data>\r\nEND\r\n
        if (strncmp(buf, "VALUE", 5) == 0) {
            char* crlf = strstr(buf, "\r\n");
            if (!crlf) return std::nullopt;
            char* data_start = crlf + 2;
            char* data_end = strstr(data_start, "\r\n");
            if (!data_end) return std::nullopt;
            return std::string(data_start, data_end - data_start);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string_view name() const noexcept override { return "Memcached"; }
};