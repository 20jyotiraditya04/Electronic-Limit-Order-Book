//
// Created by martian_duck on 25/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_ICACHECLIENT_HPP
#define ELECTRONIC_LIMIT_ORDER_BOOK_ICACHECLIENT_HPP

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_ICACHECLIENT_HPP



#pragma once
#include<iostream>
#include<string_view>
#include<string>
#include<optional>


class ICacheClient {
public:
    virtual ~ICacheClient()=default;
    virtual bool connect_to_server() noexcept=0;
    virtual void disconnect() noexcept=0;
    virtual bool set(std::string_view key, std::string_view value)noexcept=0;
    virtual std::optional<std::string> get(std::string_view key) noexcept=0;
    [[nodiscard]] virtual std::string_view name() const noexcept=0;
};