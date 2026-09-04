//
// Created by martian_duck on 15/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H
#define ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H

#pragma once
#include<cstdint>
#include<vector>
#include<algorithm>
#include<functional>
#include "OrderBookPrimitives.hpp"
#include "PriceLevelTable.hpp"

using namespace std;

struct alignas(32) TradeEvent {
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    uint32_t price;
    uint32_t qty;
    uint64_t timestamp_ns;
};

class MatchingEngine {
private:
    uint32_t min_price_;
    uint32_t max_price_;
    uint32_t tick_size_;
    size_t total_levels_;

    PriceLevelTable ladder_;
    OrderPool pool_;
    vector<Order*> order_directory_;

    void advance_best_bid(int32_t current_idx) noexcept {
        for (int32_t idx = current_idx; idx >= 0; --idx) {
            IntrusiveOrderList* level = ladder_.get_level(min_price_ + static_cast<uint32_t>(idx) * tick_size_);
            if (level && !level->empty()) {
                ladder_.set_best_bid_idx(idx);
                return;
            }
        }
        ladder_.set_best_bid_idx(-1);
    }

    void advance_best_ask(int32_t current_idx) noexcept {
        for (size_t idx = static_cast<size_t>(current_idx); idx < total_levels_; ++idx) {
            IntrusiveOrderList* level = ladder_.get_level(min_price_ + static_cast<uint32_t>(idx) * tick_size_);
            if (level && !level->empty()) {
                ladder_.set_best_ask_idx(static_cast<int32_t>(idx));
                return;
            }
        }
        ladder_.set_best_ask_idx(numeric_limits<int32_t>::max());
    }

public:
    using TradeCallback = function<void(const TradeEvent&)>;

    MatchingEngine(uint32_t min_price, uint32_t max_price, uint32_t tick_size, uint32_t max_orders)
        : min_price_(min_price),
          max_price_(max_price),
          tick_size_(tick_size),
          total_levels_(((max_price_ - min_price_) / tick_size) + 1),
          ladder_(min_price_, max_price_, tick_size_),
          pool_(max_orders),
          order_directory_(max_orders + 1, nullptr) {}

    void add_order(uint64_t order_id, uint32_t price, uint32_t qty, uint8_t side,
                   const TradeCallback& on_trade = nullptr) noexcept {
        (void)on_trade;

        Order* order = pool_.allocate();
        if (!order) {
            return;
        }

        order->order_id = order_id;
        order->price = price;
        order->qty = qty;
        order->side = side;
        order->prev = nullptr;
        order->next = nullptr;

        if (order_id < order_directory_.size()) {
            order_directory_[order_id] = order;
        }

        ladder_.add_order(order, side);

        if (side == 0) {
            int32_t best_bid_idx = ladder_.best_bid_idx();
            if (best_bid_idx >= 0) {
                advance_best_bid(best_bid_idx);
            }
        } else {
            int32_t best_ask_idx = ladder_.best_ask_idx();
            if (best_ask_idx < numeric_limits<int32_t>::max()) {
                advance_best_ask(best_ask_idx);
            }
        }
    }

    bool cancel_order(uint64_t order_id) {
        if (order_id >= order_directory_.size() || order_directory_[order_id] == nullptr) {
            return false;
        }

        Order* order = order_directory_[order_id];
        IntrusiveOrderList* level = ladder_.get_level(static_cast<uint32_t>(order->price));
        if (level) {
            level->remove(order);
        }

        order_directory_[order_id] = nullptr;
        pool_.deallocate(order);
        return true;
    }

    size_t available_pool_slots() const noexcept {
        return pool_.available();
    }

    template<uint32_t Depth = 5>
    L2snapshot<Depth> get_l2_snapshot() const noexcept {
        L2snapshot<Depth> snapshot;

        int32_t bid_idx = ladder_.best_bid_idx();
        while (bid_idx >= 0 && snapshot.bid_count < Depth) {
            uint32_t price = min_price_ + static_cast<uint32_t>(bid_idx) * tick_size_;
            IntrusiveOrderList* level = const_cast<PriceLevelTable&>(ladder_).get_level(price);
            if (level && !level->empty()) {
                uint32_t total_qty = 0;
                uint32_t count = 0;
                Order* curr = level->head();
                while (curr) {
                    total_qty += static_cast<uint32_t>(curr->qty);
                    ++count;
                    curr = curr->next;
                }
                snapshot.bids[snapshot.bid_count++] = LevelQuote{price, total_qty, count};
            }
            --bid_idx;
        }

        int32_t ask_idx = ladder_.best_ask_idx();
        while (ask_idx < static_cast<int32_t>(total_levels_) && snapshot.ask_count < Depth) {
            uint32_t price = min_price_ + static_cast<uint32_t>(ask_idx) * tick_size_;
            IntrusiveOrderList* level = const_cast<PriceLevelTable&>(ladder_).get_level(price);
            if (level && !level->empty()) {
                uint32_t total_qty = 0;
                uint32_t count = 0;
                Order* curr = level->head();
                while (curr) {
                    total_qty += static_cast<uint32_t>(curr->qty);
                    ++count;
                    curr = curr->next;
                }
                snapshot.asks[snapshot.ask_count++] = LevelQuote{price, total_qty, count};
            }
            ++ask_idx;
        }

        return snapshot;
    }
};

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H