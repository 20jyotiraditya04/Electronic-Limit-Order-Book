//
// Created by martian_duck on 04/09/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_TRADEEVENT_HPP
#define ELECTRONIC_LIMIT_ORDER_BOOK_TRADEEVENT_HPP

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_TRADEEVENT_HPP



#pragma once
#include <cstdint>
#include <chrono>
#include <string_view>
#include <cstring>

#pragma pack(push, 1)

enum class EventType : uint8_t {
    ORDER_PLACED    = 1,
    ORDER_EXECUTED  = 2,
    ORDER_CANCELLED = 3
};

// Emitted when two orders match (Execution Fill)
struct alignas(8) TradeEvent {
    uint64_t timestamp_ns{0};   // Monotonic hardware timestamp
    uint64_t match_id{0};       // Unique execution match ID
    uint64_t maker_order_id{0}; // Resting Maker Order ID
    uint64_t taker_order_id{0}; // Aggressive Taker Order ID
    uint32_t price{0};          // Execution price tick
    uint32_t qty{0};            // Matched quantity
    uint8_t  taker_side{0};     // 0 = Buy, 1 = Sell

    // Zero-copy binary serialization helper
    [[nodiscard]] std::string_view as_binary_view() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(this), sizeof(TradeEvent));
    }
};

// Emitted when an order enters the book or is cancelled
struct alignas(8) OrderLifecycleEvent {
    uint64_t  timestamp_ns{0};
    uint64_t  order_id{0};
    uint32_t  price{0};
    uint32_t  qty{0};
    uint8_t   side{0};          // 0 = Buy, 1 = Sell
    EventType event_type{EventType::ORDER_PLACED};

    // Zero-copy binary serialization helper
    [[nodiscard]] std::string_view as_binary_view() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(this), sizeof(OrderLifecycleEvent));
    }
};

#pragma pack(pop)