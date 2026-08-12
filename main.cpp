#include "OrderBookPrimitives.hpp"
#include<cstddef>
#include<cstdint>
#include<vector>
#include<cassert>
#include<iostream>


using namespace std;

int main()
{
    OrderPool pool(10000000);
    IntrusiveOrderList price_level;

    cout<<"Initial Pool Available Order: "<<pool.available()<<endl;

    Order* o1 = pool.allocate();
    o1->order_id=101;
    o1->price=10000;
    o1->qty=50;
    price_level.push_back(o1);

    Order* o2 =pool.allocate();
    o2->order_id=102;
    o2->price=10000;
    o2->qty=30;
    price_level.push_back(o2);

    Order* o3 = pool.allocate();
    o3->order_id=103;
    o3->price=10000;
    o3->qty=20;
    price_level.push_back(o3);


    cout<<"Queue size after 3 additions: "<<price_level.size()<<endl;
    cout<<"Pool Available Order after 3 additions: "<<pool.available()<<endl;

    price_level.remove(o2);
    pool.deallocate(o2);
    cout<<"Queue size after middle cancellation: "<<price_level.size()<<endl;
    cout<<"Pool Available Slots recycled: "<<pool.available()<<endl;


    return 0;
}