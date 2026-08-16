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
                return;
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
                if (best_ask_idx<0 || static_cast<size_t>(best_ask_idx)>total_levels_)break;

                uint32_t best_ask_price=min_price_+(best_ask_idx*tick_size_);
                IntrusiveOrderList* ask_level = ladder_.get_best_ask_level();
                if (!ask_level || ask_level->empty()) {
                    advance_best_ask(best_ask_idx+1);
                    continue;
                }

                while (!ask_level->empty() && remaining_qty>0) {
                    Order* order = ask_level->head();
                    uint32_t match_qty=min(remaining_qty,maker_qty);
                    if (on_trade) {
                        on_trade(TradeEvent{
                            .maker_order_id=order->maker_order_id,
                            .taker_order_id = order_id,
                            .price=maker_price,
                            .qty=match_qty,
                            .timestamp_ns=0
                        });
                    }
                    remaining_qty-=match_qty;
                    maker->qty-=match_qty;


                    if (maker->qty==0) {
                        ask_level->pop_front();
                        if (maker->order_id < order_directory_.size()) {
                            order_directory_[maker->order_id]=nullptr;
                        }
                        pool_.deallocate(maker);
                    }
                }
                if (ask_level->empty()) {
                    advance_best_ask(best_ask_idx+1);
                }
            }
        }
        else {
            while (remaining_qty>0) {
                int32_t best_bid_idx=ladder_.best_bid_idx();
                if (best_bid_idx<0)break;
                uint32_t best_bid_price=min_price+(best_bid_idx*tick_size_);
                if (price>best_bid_price)break;
                IntrusiveOrderList* bid_level = ladder_.get_best_bid_level();
                if (!bid_level->empty() && remaining_qty>0) {
                    advance_best_bid(best_bid_idx-1);
                    continue;
                }
                while (!bid_level->empty() && remaining_qty>0) {
                    Order* order =bid_level->head();
                    uint32_t match_qty=min(remaining_qty,maker_qty);
                    if (on_trade) {
                        on_trade(TradeEvent{
                            .maker_order_id = maker->maker_order_id,
                            .taker_order_id = order->id,
                            .price = maker->price,
                            .qty=match_qty,
                            .timestamp_ns=0
                        });
                    }
                    remaining_qty-=match_qty;
                    maker->qty-=match_qty;
                    if (maker->qty==0) {
                        bid_level->pop_front();
                        if (maker->order_id<order_directory_.size()) {
                            order_directory_[maker->order_id]=nullptr;
                        }
                        pool_.deallocate(maker);
                    }
                }
                if (bid_level->empty()) {
                    advance_best_bid(best_bid_idx-1);
                }
            }
        }
        if (remaining_qty>0) {
            Order* order = pool_.allocate();
            if (resting_order) {
                resting_order->order_id=order_id;
                resting_order->price=price;
                resting_order->qty=remaining_qty;
                resting_order->side=side;

                ladder_.add_order(resting_order,side);
                if (order_id<order_directory_.size()) {
                    order_directory_[order_id]=resting_order;
                }
            }
        }
    }
    bool cancel_order(uint64_t order_id) {
        if (order_id>=order_directory_.size() || order_directory_[order_id]==nullptr) {
            return false;
        }
        Order* order=order_directory_[order_id];
        IntrusiveOrderList* level=ladder_.get_level(order->price);
        if (level) {
            level->remove(order);
        }
        order_directory_[order_id]=nullptr;
        pool_.deallocate(order);
        return true;
    }

    size_t available_pool_slots() const noexcept {
        return pool_.available();
    }

    template<uint32_t Depth=5>
    L2snapshot<Depth> get_l2_snapshot() const noexcept{
        L2snapshot<Depth> snapshot;
        uint32_t bid_idx=ladder_.best_bid_idx();
        while (bid_idx>=0 && bid_idx<=best_bid_idx_) {
            uint32_t price=min_price+(bid_idx*tick_size_);
            const IntrusiveOrderList* level=const_cast<PriceLevelTable&>(ladder_).get_level(price);

            if (level && !level->empty()) {
                uint32_t total_qty=0;
                uint32_t count=0;
                const Order* curr=level->head();
                while (curr) {
                    total_qty+=curr->qty;
                    count++;
                    curr=curr->next;
                }
                snapshot.bids[snapshot.bid_count++]=LevelQuote{price,total_qty,count};
            }
            bid_idx--;
        }

        uint32_t ask_idx=ladder_.best_ask_idx();
        while (ask_idx>=0 && ask_idx<=best_ask_idx_) {
            uint32_t price=min_price+(ask_idx*tick_size_);
            const IntrusiveOrderList* level=const_cast<PriceLevelTable&>(ladder_).get_level(price);
            if (level && !level->empty()) {
                uint32_t total_qty=0;
                uint32_t count=0;
                const Order* curr=level->head;
                while (curr) {
                    total_qty+=curr_>qty;
                    count++;
                    curr=curr->next;
                }
                snapshot.bids[snapshot.ask_count++]=LevelQuote{price,total_qty,count};
            }
            ask_idx++;
        }

        return snapshot;
    }





};