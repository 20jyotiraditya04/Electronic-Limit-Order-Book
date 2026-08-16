//
// Created by martian_duck on 24/08/26.
//

#ifndef ELECTRONIC_LIMIT_ORDER_BOOK_L2SNAPSHOT_H
#define ELECTRONIC_LIMIT_ORDER_BOOK_L2SNAPSHOT_H

#endif //ELECTRONIC_LIMIT_ORDER_BOOK_L2SNAPSHOT_H


#pragma once
#include <cstdint>
#include<iostream>
#include<limits>
#include<cstddef>
#include<array>
#include<cstring>
#include<string_view>
#include<charconv>


struct LevelQuote {
    uint32_t price{0};
    uint32_t total_qty{0};
    uint32_t order_count{0};
};


template<size_t Depth=5>
struct L2snapshot {
    uint64_t timestamp_ns{0};
    size_t bid_count{0};
    size_t ask_count{0};

    std::array<LevelQuote,Depth> bids;
    std::array<LevelQuote,Depth> asks;

    /*
     *            FAST ZERO ALLOCATION STRING ALLOCATION
     */
    //Output Format B:(price:qty),(price:qty) | A:(price,qty),(price:qty)

    size_t Serialize_to_Buffer(char* buffer,size_t max_length) const noexcept {
        char* ptr=buffer;
        char* end=ptr+max_length;

        auto append_str=[&](std::string_view sv) {
            if (ptr+sv.size()<end) {
                std::memcpy(ptr,sv.data(),sv.size());
                ptr=ptr+sv.size();
            }
        };

        auto append_uint=[&](uint32_t val) {
            auto [p,ec]=std::to_chars(ptr,end,val);
            if (ec==std::errc()) {
                ptr=p;
            }
        };


        append_str("B|:");
        for (int i=0;i<bid_count;i++) {
            if (i>0) {
                append_str(",");
            }
            append_uint(bids[i].price);
            append_str(":");
            append_uint(bids[i].total_qty);
        }
        append_str("A:");
        for (int i=0;i<ask_count;i++) {
            if (i>0) {
                append_str(",");
            }
            append_uint(asks[i].price);
            append_str(":");
            append_uint(asks[i].total_qty);
        }

        if (ptr<end) {
            *ptr='\0';
        }


        return static_cast<size_t>(ptr-buffer);
    }

};