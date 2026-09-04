//
// Created by martian_duck on 25/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_REDISCACHECLIENT_HPP
#define ELECTRONIC_LIMIT_ORDER_BOOK_REDISCACHECLIENT_HPP

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_REDISCACHECLIENT_HPP

#pragma once
#include<string>
#include<string_view>
#include<optional>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include "ICacheClient.hpp"


class RedisCacheClient:public ICacheClient {
private:
    std::string host_;
    int port_{0};
    std::string client_name_;
    int sock_fd_{0};
public:
    RedisCacheClient(std::string host,int port, std::string_view client_name="Valkey/Redis"):host_(std::move(host)),port_(port),client_name_(std::move(client_name)){}
    ~RedisCacheClient()override {
        disconnect();
    }
    bool connect_to_server() noexcept override {
        sock_fd_=socket(AF_INET,SOCK_STREAM,0);
        if (sock_fd_<0) return false;
        sockaddr_in serv_addr{};
        serv_addr.sin_family=AF_INET;
        serv_addr.sin_port=htons(port_);
        if (inet_pton(AF_INET,host_.c_str(),&serv_addr.sin_addr)<=0) {
            disconnect();
            return false;
        }
        if (connect(sock_fd_,reinterpret_cast<sockaddr*>(&serv_addr),sizeof(serv_addr))<0) {
            disconnect();
            return false;
        }
        return true;
    }
    void disconnect() noexcept override {
        if (sock_fd_>=0) {
            close(sock_fd_);
            sock_fd_=-1;
        }
    }
    bool set(std::string_view key, std::string_view value) noexcept override {
        if (sock_fd_<0)return false;
        std::string req="*3\r\n$3\r\nSET\r\n$" + std::to_string(key.size()) + "\r\n" +
                          std::string(key) + "\r\n$" + std::to_string(value.size()) + "\r\n" +
                          std::string(value) + "\r\n";
        if (write(sock_fd_,req.data(),req.size())<0) {
            return false;
        }
        char buf[64];
        ssize_t n=read(sock_fd_,buf,sizeof(buf)-1);
        if (n<=0)return false;
        buf[n]='\0';
        return buf[0]=='+';
    }
    std::optional<std::string> get(std::string_view key)noexcept override {
        if (sock_fd_<0)return std::nullopt;
        std::string req = "*2\r\n$3\r\nGET\r\n$" + std::to_string(key.size()) + "\r\n" +
                          std::string(key) + "\r\n";
        if (write(sock_fd_,req.data(),req.size())<0) {return std::nullopt;}
        char buf[1024];
        ssize_t n=read(sock_fd_,buf,sizeof(buf)-1);
        if (n<=0)return std::nullopt;
        buf[n]='\0';
        if (buf[0]=='$'){
            if (buf[1] == '-' && buf[2] == '1') return std::nullopt; // Key not found ($-1)
            char* crlf = strstr(buf, "\r\n");
            if (!crlf) return std::nullopt;
            char* data_start = crlf + 2;
            char* data_end = strstr(data_start, "\r\n");
            if (!data_end) return std::nullopt;
            return std::string(data_start, data_end - data_start);
        }
        return std::nullopt;
    }
    [[nodiscard]] std::string_view name() const noexcept override {
        return client_name_;
    }
};