//
// Created by martian_duck on 25/08/26.
//



#include <iostream>
#include <cassert>
#include "../include/MatchingEngine.hpp"
#include "../include/L2Snapshot.hpp"

int main() {
    std::cout << "===================================================\n";
    std::cout << "     L2 SNAPSHOT EXTRACTION & SERIALIZATION TEST   \n";
    std::cout << "===================================================\n";

    MatchingEngine engine(10000, 10500, 1, 10000);

    // Populate resting liquidity
    // Bids: 10040 (qty 30), 10045 (qty 50)
    engine.add_order(1, 10045, 50, 0);
    engine.add_order(2, 10040, 30, 0);

    // Asks: 10050 (qty 20 + 30 = 50), 10055 (qty 80)
    engine.add_order(3, 10050, 20, 1);
    engine.add_order(4, 10050, 30, 1);
    engine.add_order(5, 10055, 80, 1);

    // Extract Top-5 L2 Depth Snapshot
    auto snapshot = engine.get_l2_snapshot<5>();

    assert(snapshot.bid_count == 2);
    assert(snapshot.bids[0].price == 10045 && snapshot.bids[0].total_qty == 50);
    assert(snapshot.bids[1].price == 10040 && snapshot.bids[1].total_qty == 30);

    assert(snapshot.ask_count == 2);
    assert(snapshot.asks[0].price == 10050 && snapshot.asks[0].total_qty == 50 && snapshot.asks[0].order_count == 2);
    assert(snapshot.asks[1].price == 10055 && snapshot.asks[1].total_qty == 80);

    // Zero-allocation serialization into a stack buffer
    char buffer[256];
    size_t written = snapshot.serialize_to_buffer(buffer, sizeof(buffer));

    std::cout << "Extracted Snapshot String:\n  " << buffer << "\n";
    std::cout << "Payload Length: " << written << " bytes\n";
    std::cout << "\n>>> SUB-PHASE 2A TESTS PASSED SUCCESSFULLY! <<<\n";

    return 0;
}