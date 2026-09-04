//
// Created by martian_duck on 15/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H
#define ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H

#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include "OrderBookPrimitives.hpp"
#include "PriceLevelTable.hpp"
#include "TradeEvent.hpp"
#include "L2Snapshot.hpp"

// Non-allocating zero-overhead callback signatures
using TradeCallback = void(*)(const TradeEvent&, void* context);
using LifecycleCallback = void(*)(const OrderLifecycleEvent&, void* context);

class MatchingEngine {
private:
    uint32_t min_price_;
    uint32_t max_price_;
    uint32_t tick_size_;
    size_t total_levels_;

    PriceLevelTable ladder_;
    OrderPool pool_;
    std::vector<Order*> order_directory_;

    uint64_t next_match_id_{1};

    // Sub-Phase 3A: Event dissemination callbacks
    TradeCallback trade_cb_{nullptr};
    void* trade_ctx_{nullptr};
    LifecycleCallback lifecycle_cb_{nullptr};
    void* lifecycle_ctx_{nullptr};

    [[nodiscard]] inline uint64_t current_time_ns() const noexcept {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

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
        ladder_.set_best_ask_idx(std::numeric_limits<int32_t>::max());
    }

public:
    MatchingEngine(uint32_t min_price, uint32_t max_price, uint32_t tick_size, uint32_t max_orders)
        : min_price_(min_price),
          max_price_(max_price),
          tick_size_(tick_size),
          total_levels_(((max_price_ - min_price_) / tick_size) + 1),
          ladder_(min_price_, max_price_, tick_size_),
          pool_(max_orders),
          order_directory_(max_orders + 1, nullptr) {}

    // Event listener registration
    void set_trade_callback(TradeCallback cb, void* ctx = nullptr) noexcept {
        trade_cb_ = cb;
        trade_ctx_ = ctx;
    }

    void set_lifecycle_callback(LifecycleCallback cb, void* ctx = nullptr) noexcept {
        lifecycle_cb_ = cb;
        lifecycle_ctx_ = ctx;
    }

    // =========================================================================
    // Continuous Double Auction Order Matching Loop
    // =========================================================================
    bool add_order(uint64_t order_id, uint32_t price, uint32_t qty, uint8_t side) noexcept {
        if (price < min_price_ || price > max_price_ || qty == 0) [[unlikely]] {
            return false;
        }

        uint32_t remaining_qty = qty;

        // 1. Crossing / Matching Sweep
        if (side == 0) {
            // Incoming BUY Order: Cross against resting ASKs (lowest ask first)
            while (remaining_qty > 0) {
                int32_t best_ask_idx = ladder_.best_ask_idx();
                if (best_ask_idx >= static_cast<int32_t>(total_levels_) ||
                    best_ask_idx == std::numeric_limits<int32_t>::max()) {
                    break; // No sellers available
                }

                uint32_t best_ask_price = min_price_ + (static_cast<uint32_t>(best_ask_idx) * tick_size_);
                if (price < best_ask_price) {
                    break; // Price does not cross
                }

                IntrusiveOrderList* level = ladder_.get_level(best_ask_price);
                if (!level || level->empty()) {
                    advance_best_ask(best_ask_idx + 1);
                    continue;
                }

                // Match FIFO orders at this price level
                while (!level->empty() && remaining_qty > 0) {
                    Order* maker = level->head();
                    uint32_t match_qty = std::min(remaining_qty, static_cast<uint32_t>(maker->qty));

                    remaining_qty -= match_qty;
                    maker->qty -= match_qty;

                    // Emit trade execution event
                    if (trade_cb_) [[likely]] {
                        TradeEvent event{
                            .timestamp_ns   = current_time_ns(),
                            .match_id       = next_match_id_++,
                            .maker_order_id = maker->order_id,
                            .taker_order_id = order_id,
                            .price          = static_cast<uint32_t>(maker->price),
                            .qty            = match_qty,
                            .taker_side     = side
                        };
                        trade_cb_(event, trade_ctx_);
                    }

                    // Fully filled maker order: unhook and return to pool
                    if (maker->qty == 0) {
                        level->remove(maker);
                        if (maker->order_id < order_directory_.size()) {
                            order_directory_[maker->order_id] = nullptr;
                        }
                        pool_.deallocate(maker);
                    }
                }

                // Advance best ask index if level was cleared
                if (level->empty()) {
                    advance_best_ask(best_ask_idx + 1);
                }
            }
        } else {
            // Incoming SELL Order: Cross against resting BIDS (highest bid first)
            while (remaining_qty > 0) {
                int32_t best_bid_idx = ladder_.best_bid_idx();
                if (best_bid_idx < 0) {
                    break; // No buyers available
                }

                uint32_t best_bid_price = min_price_ + (static_cast<uint32_t>(best_bid_idx) * tick_size_);
                if (price > best_bid_price) {
                    break; // Price does not cross
                }

                IntrusiveOrderList* level = ladder_.get_level(best_bid_price);
                if (!level || level->empty()) {
                    advance_best_bid(best_bid_idx - 1);
                    continue;
                }

                // Match FIFO orders at this price level
                while (!level->empty() && remaining_qty > 0) {
                    Order* maker = level->head();
                    uint32_t match_qty = std::min(remaining_qty, static_cast<uint32_t>(maker->qty));

                    remaining_qty -= match_qty;
                    maker->qty -= match_qty;

                    // Emit trade execution event
                    if (trade_cb_) [[likely]] {
                        TradeEvent event{
                            .timestamp_ns   = current_time_ns(),
                            .match_id       = next_match_id_++,
                            .maker_order_id = maker->order_id,
                            .taker_order_id = order_id,
                            .price          = static_cast<uint32_t>(maker->price),
                            .qty            = match_qty,
                            .taker_side     = side
                        };
                        trade_cb_(event, trade_ctx_);
                    }

                    // Fully filled maker order: unhook and return to pool
                    if (maker->qty == 0) {
                        level->remove(maker);
                        if (maker->order_id < order_directory_.size()) {
                            order_directory_[maker->order_id] = nullptr;
                        }
                        pool_.deallocate(maker);
                    }
                }

                // Advance best bid index if level was cleared
                if (level->empty()) {
                    advance_best_bid(best_bid_idx - 1);
                }
            }
        }

        // 2. Residual Placement: Rest remaining quantity on the ladder
        if (remaining_qty > 0) {
            Order* order = pool_.allocate();
            if (!order) [[unlikely]] {
                return false; // Order pool exhausted
            }

            order->order_id = order_id;
            order->price    = price;
            order->qty      = remaining_qty;
            order->side     = side;
            order->prev     = nullptr;
            order->next     = nullptr;

            if (order_id < order_directory_.size()) {
                order_directory_[order_id] = order;
            }

            ladder_.add_order(order, side);

            // Update top-of-book indices if this new order creates a new best price
            if (side == 0) {
                int32_t idx = static_cast<int32_t>((price - min_price_) / tick_size_);
                if (idx > ladder_.best_bid_idx()) {
                    ladder_.set_best_bid_idx(idx);
                }
            } else {
                int32_t idx = static_cast<int32_t>((price - min_price_) / tick_size_);
                if (idx < ladder_.best_ask_idx()) {
                    ladder_.set_best_ask_idx(idx);
                }
            }

            // Emit Order Placed event
            if (lifecycle_cb_) [[likely]] {
                OrderLifecycleEvent event{
                    .timestamp_ns = current_time_ns(),
                    .order_id     = order_id,
                    .price        = price,
                    .qty          = remaining_qty,
                    .side         = side,
                    .event_type   = EventType::ORDER_PLACED
                };
                lifecycle_cb_(event, lifecycle_ctx_);
            }
        }

        return true;
    }

    // =========================================================================
    // O(1) Cancellation via Direct Directory Lookup
    // =========================================================================
    bool cancel_order(uint64_t order_id) noexcept {
        if (order_id >= order_directory_.size() || order_directory_[order_id] == nullptr) {
            return false;
        }

        Order* order = order_directory_[order_id];
        uint32_t order_price = static_cast<uint32_t>(order->price);
        uint32_t order_qty   = static_cast<uint32_t>(order->qty);
        uint8_t  order_side  = static_cast<uint8_t>(order->side);

        IntrusiveOrderList* level = ladder_.get_level(order_price);
        if (level) {
            level->remove(order);

            // If level becomes empty, advance best tracker
            if (level->empty()) {
                int32_t idx = static_cast<int32_t>((order_price - min_price_) / tick_size_);
                if (order_side == 0 && idx == ladder_.best_bid_idx()) {
                    advance_best_bid(idx - 1);
                } else if (order_side == 1 && idx == ladder_.best_ask_idx()) {
                    advance_best_ask(idx + 1);
                }
            }
        }

        order_directory_[order_id] = nullptr;
        pool_.deallocate(order);

        // Emit Order Cancelled event
        if (lifecycle_cb_) [[likely]] {
            OrderLifecycleEvent event{
                .timestamp_ns = current_time_ns(),
                .order_id     = order_id,
                .price        = order_price,
                .qty          = order_qty,
                .side         = order_side,
                .event_type   = EventType::ORDER_CANCELLED
            };
            lifecycle_cb_(event, lifecycle_ctx_);
        }

        return true;
    }

    [[nodiscard]] size_t available_pool_slots() const noexcept {
        return pool_.available();
    }

    // =========================================================================
    // Level 2 (L2) Depth Snapshot Extraction
    // =========================================================================
    template<uint32_t Depth = 5>
    [[nodiscard]] L2snapshot<Depth> get_l2_snapshot() const noexcept {
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

#endif // ELECTRONIC_LIMIT_ORDER_BOOK_MATCHINGENGINE_H