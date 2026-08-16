//
// Created by martian_duck on 15/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H
#define ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H


#pragma once
#include<cstdint>
#include<vector>
#include<algorithm>
#include<functional>
#include "OrderBookPrimitives.hpp"
#include "PriceLevelTable.hpp"

using namespace std;

struct alignas(32) TradeEvent{
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

    void advance_best_bid(int32_t current_idx) noexcept{
        for (int32_t idx=current_idx; idx>=0 ; --idx) {
            IntrusiveOrderList* level = ladder_.get_level(min_price_+idx*tick_size_);
            if (level && !level->empty()) {
                ladder_.set_best_bid_idx(idx);
            }
        }
        ladder_.set_best_bid_idx(-1);
    }

    void advance_best_ask(int32_t current_idx) noexcept {
        for (size_t idx=static_cast<size_t>(current_idx); idx<total_levels_; ++idx) {
            IntrusiveOrderList* level = ladder_.get_level(min_price_+idx*tick_size_);
            if (level && !level->empty()) {
                ladder_.set_best_ask_idx(static_cast<int32_t>(idx));
                return;
            }
        }
        ladder_.set_best_ask_idx(numeric_limits<int32_t>::max());
    }

public:
    using TradeCallback = function<void(const TradeEvent&)>;
    MatchingEngine(uint32_t min_price, uint32_t max_price, uint32_t tick_size, uint32_t max_orders):min_price_(min_price),max_price_(max_price),tick_size_(tick_size),total_levels_(((max_price_-min_price_)/tick_size)+1),ladder_(min_price_,max_price_,tick_size_),pool_(max_orders),order_directory_(max_orders+1,nullptr) {
    }

    //HOT PATH
    void add_order(uint64_t order_id, uint32_t price, uint32_t qty, uint8_t side, const TradeCallback& on_trade=nullptr) noexcept {
        uint32_t remaining_qty=qty;
        /*
         *
         *1.MATCHING LOOP (CROSS OPPOSITE SIDE)
         *
        */
        if (side == 0) {
            while (remaining_qty>0) {
                int32_t best_ask_idx = ladder_.best_ask_idx();
                if (best_ask_idx<0 || static_cast<size_t>(best_ask_idx)>=total_levels_)break;

                uint32_t best_ask_price=min_price_+(best_ask_idx*tick_size_);
                IntrusiveOrderList* ask_level = ladder_.get_best_ask_level();
                if (!ask_level || ask_level->empty()) {
                    advance_best_ask(best_ask_idx+1);
                    continue;
                }


                

            }
        }
    }







};