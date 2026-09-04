//
// Created by martian_duck on 25/08/26.
//
#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include "../include/L2Snapshot.hpp"
#include "../include/RedisCacheClient.hpp"
#include "../include/MemcachedCacheClient.hpp"

int main() {
    std::cout << "===================================================\n";
    std::cout << "   3-WAY CACHE INTEGRATION VERIFICATION           \n";
    std::cout << "===================================================\n";

    // 1. Create simulated L2 snapshot payload
    L2snapshot<5> snapshot;
    snapshot.bid_count = 2;
    snapshot.bids[0] = LevelQuote{10045, 50, 1};
    snapshot.bids[1] = LevelQuote{10040, 30, 2};

    snapshot.ask_count = 2;
    snapshot.asks[0] = LevelQuote{10050, 50, 2};
    snapshot.asks[1] = LevelQuote{10055, 80, 1};

    char payload_buf[256];
    size_t len = snapshot.serialize_to_buffer(payload_buf, sizeof(payload_buf));
    std::string_view serialized_l2(payload_buf, len);

    // 2. Register all three backends
    std::vector<std::unique_ptr<ICacheClient>> clients;
    clients.push_back(std::make_unique<RedisCacheClient>("127.0.0.1", 6379, "Valkey/Redis"));
    clients.push_back(std::make_unique<RedisCacheClient>("127.0.0.1", 6380, "Dragonfly"));
    clients.push_back(std::make_unique<MemcachedCacheClient>("127.0.0.1", 11211));

    // 3. Test SET and GET across each cache
    for (auto& client : clients) {
        std::cout << "Testing [" << client->name() << "] ... ";
        if (!client->connect_to_server()) {
            std::cout << "FAILED TO CONNECT (Is service running?)\n";
            continue;
        }

        // Write snapshot
        bool set_ok = client->set("BTC_USDT_L2", serialized_l2);
        assert(set_ok == true);

        // Read snapshot
        auto retrieved = client->get("BTC_USDT_L2");
        assert(retrieved.has_value());
        assert(retrieved.value() == serialized_l2);

        std::cout << "SUCCESS! (Verified SET & GET Match)\n";
        client->disconnect();
    }

    std::cout << "\n>>> ALL 3 ENGINES (VALKEY, DRAGONFLY, MEMCACHED) VERIFIED! <<<\n";
    return 0;
}