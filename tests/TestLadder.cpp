//
// Created by martian_duck on 14/08/26.
//



#include <iostream>
#include "../include/PriceLevelTable.hpp"

int main() {
    PriceLevelTable ladder(10000, 10500, 1);
    OrderPool pool(100);

    Order* b1=pool.allocate();
    b1->order_id=1;
    b1->price=10500;
    b1->qty=100;
    b1->side=0;
    ladder.add_order(b1,b1->side);

    Order* b2=pool.allocate();
    b2->order_id=2;
    b2->price=10075;
    b2->qty=50;
    b2->side=0;
    ladder.add_order(b2,b2->side);

    IntrusiveOrderList* best_bid=ladder.get_best_bid_level();
    cout<<"Best Bid Queue Size: "<<best_bid->size()<<endl;
    cout<<"Best Bid Order ID: "<<best_bid->head()->qty<<endl;
    return 0;

}