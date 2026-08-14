//
// Created by martian_duck on 14/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_PRICELEVELTABLE_HPP
#define ELECTRONIC_LIMIT_ORDER_BOOK_PRICELEVELTABLE_HPP

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_PRICELEVELTABLE_HPP


#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include <limits>
#include <iostream>

#include "OrderBookPrimitives.hpp"

using namespace std;


class PriceLevelTable {
private:
    uint32_t min_price_;
    uint32_t max_price_;
    uint32_t tick_size;
    size_t total_levels_;
    vector<IntrusiveOrderList> levels_;
    int32_t best_bid_idx_{-1};
    int32_t best_ask_idx_{-1};
    inline int32_t price_to_idx(uint32_t price) const noexcept {
        if (price<min_price_ || price>max_price_) {
            return -1;
        }
        return static_cast<int32_t>((price-min_price_)/tick_size);
    }
public:
    PriceLevelTable(uint32_t min_price, uint32_t max_price, uint32_t tick_size_):min_price_(min_price),max_price_(max_price),tick_size_(tick_size) {
        assert(max_price_>min_price);
        assert(tick_size_>0);

        total_levels_=((max_price_-min_price_)/tick_size);
        levels_.resize(total_levels_);
    }

    IntrusiveOrderList* get_level(uint32_t price) noexcept
    {
        int32_t idx = price_to_idx(price);
        if (idx<0 || static_cast<size_t>(idx)>=total_levels_) {
            return nullptr;
        }
        return &levels_[idx];
    }

    bool add_order(Order* order,uint8_t side) noexcept
    {
        int32_t idx=price_to_idx(order->price);
        if (idx<0 || static_cast<size_t>(idx) >=total_levels_) {
            return false;
        }
        levels_[idx].push_back(order);

        if (side==0) {
            if (idx>best_bid_idx_) {
                best_bid_idx_=idx;
            }
        }
        else {
            if (idx<best_ask_idx_) {
                best_ask_idx=idx;
            }
        }
        return true;
    }
    int32_t best_bid_idx() const noexcept {
        return best_bid_idx_;
    }
    int32_t best_ask_idx() const noexcept {
        return best_ask_idx_;
    }
    IntrusiveOrderList* get_best_bid_level() noexcept {
        if (best_bid_idx()<0) {
            return nullptr;
        }
        return &levels_[best_bid_idx_];

    }
    IntrusiveOrderList* get_best_ask_level() noexcept {
        if (best_ask_idx_ == numeric_limits<int32_t>::max())return nullptr;
        return &levels_[best_ask_idx_];
    }

};