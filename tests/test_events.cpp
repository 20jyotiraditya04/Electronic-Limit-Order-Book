//
// Created by martian_duck on 04/09/26.
//


#include <iostream>
#include <vector>
#include <cassert>
#include "../include/MatchingEngine.hpp"
#include "../include/TradeEvent.hpp"

// Event collectors
std::vector<TradeEvent> recorded_trades;
std::vector<OrderLifecycleEvent> recorded_lifecycle;

void on_trade(const TradeEvent& event, void* /*ctx*/) {
    recorded_trades.push_back(event);
}

void on_lifecycle(const OrderLifecycleEvent& event, void* /*ctx*/) {
    recorded_lifecycle.push_back(event);
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "   SUB-PHASE 3A: BINARY EVENT STREAM VERIFICATION  \n";
    std::cout << "===================================================\n";

    MatchingEngine engine(10000, 10500, 1, 10000);
    engine.set_trade_callback(on_trade);
    engine.set_lifecycle_callback(on_lifecycle);

    // 1. Post Maker Sell Order (resting in book)
    // OrderID: 101, Price: 10050, Qty: 100, Side: 1 (Sell)
    engine.add_order(101, 10050, 100, 1);

    assert(recorded_lifecycle.size() == 1);
    assert(recorded_lifecycle[0].event_type == EventType::ORDER_PLACED);
    assert(recorded_lifecycle[0].order_id == 101);
    assert(recorded_lifecycle[0].qty == 100);

    // 2. Aggressive Taker Buy Order (Causes Partial Fill)
    // OrderID: 202, Price: 10050, Qty: 40, Side: 0 (Buy)
    engine.add_order(202, 10050, 40, 0);

    assert(recorded_trades.size() == 1);
    assert(recorded_trades[0].match_id == 1);
    assert(recorded_trades[0].maker_order_id == 101);
    assert(recorded_trades[0].taker_order_id == 202);
    assert(recorded_trades[0].price == 10050);
    assert(recorded_trades[0].qty == 40);
    assert(recorded_trades[0].taker_side == 0);

    // Verify zero-copy binary serialization size
    std::string_view bin_trade = recorded_trades[0].as_binary_view();
    assert(bin_trade.size() == sizeof(TradeEvent));

    // 3. Cancel remaining shares of Maker Order
    bool cancelled = engine.cancel_order(101);
    assert(cancelled);

    assert(recorded_lifecycle.size() == 2);
    assert(recorded_lifecycle[1].event_type == EventType::ORDER_CANCELLED);
    assert(recorded_lifecycle[1].order_id == 101);
    assert(recorded_lifecycle[1].qty == 60); // 100 - 40 executed = 60 remaining

    std::cout << "Recorded Trades: " << recorded_trades.size() << "\n";
    std::cout << "Trade Binary Payload Size: " << sizeof(TradeEvent) << " bytes\n";
    std::cout << "Recorded Lifecycle Events: " << recorded_lifecycle.size() << "\n";
    std::cout << "Lifecycle Binary Payload Size: " << sizeof(OrderLifecycleEvent) << " bytes\n";

    std::cout << "\n>>> SUB-PHASE 3A COMPLETE: BINARY EVENT STREAM PASSES! <<<\n";
    return 0;
}