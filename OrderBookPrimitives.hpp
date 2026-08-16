#include<cstdint>
#include<cstddef>
#include<vector>
#include<cassert>
#include<iostream>

using namespace std;

///////////////////////////////////////////
////
////   CACHE ALINGED ORDER CONSTRUCT 
////
///////////////////////////////////////////
struct alignas(64) Order {
    uint64_t order_id{0};
    uint64_t price{0};
    uint64_t qty{};
    Order* prev{nullptr};
    Order* next{nullptr};


    uint64_t timestamp_ns{0};
    uint32_t acc_id{0};
    uint8_t side{0};            // 0-buy, 1-sell
    uint8_t padding[19][0];
};


static_assert(sizeof(Order) == 64, "Order struct must be 64 bytes");
/// @brief  Slab memory Pool Allocator for Order objects
class OrderPool{
    private:
        vector<Order> pool_;
        vector<uint32_t> free_stack_;
        uint32_t capacity_;
    public:
        explicit OrderPool(uint32_t max_orders) : capacity_(max_orders)
        {
            pool_.resize(capacity_);
            free_stack_.reserve(capacity_);



            for(uint32_t i=0;i<capacity_;++i)
            {
                free_stack_.push_back(capacity_-1-i);   
            }

        }

        Order* allocate(){
            if(free_stack_.empty()) 
            {
                return nullptr;
            }

            uint32_t index = free_stack_.back();
            free_stack_.pop_back();

            Order* order=&pool_[index];
            *order=Order();
            return order;

        }

        void deallocate(Order* order)
        {
            assert(order >= &pool_[0] && order <= &pool_[capacity_-1]);
            uint32_t index =static_cast<uint32_t>(order-&pool_[0]);
            free_stack_.push_back(index);
        }

        size_t available() const noexcept{
            return free_stack_.size();
        }

};



///// INTRUSIVE DOUBLY LINKED LIST (FIFO QUEUE)

class IntrusiveOrderList{
    private:
        Order* head_{nullptr};
        Order* tail_{nullptr};
        uint32_t count{0};
    public:
        IntrusiveOrderList() =default;
        bool empty() const noexcept{
            return head_ == nullptr;
        }
        uint32_t size() const noexcept{
            return count;
        }
        Order* head() const noexcept{
            return head_;
        }

        void push_back(Order* order) noexcept{
            order->next =nullptr;
            order->prev = tail_;
            if(tail_){
                tail_->next=order;
            }
            else
            {
                head_=order;
            }
            tail_=order;
            ++count;
        }


        void remove(Order* order) noexcept{
            if(order->prev)
            {
                order->prev->next=order->next;
            }
            else
            {
                head_=order->next;
            }

            if(order->next)
            {
                order->next->prev=order->prev;
            }
            else
            {
                tail_=order->prev;
            }

            order->next=nullptr;
            order->prev=nullptr;
            --count;
        }


        Order* pop_front() noexcept{
            if(!head_)
            {
                return nullptr;
            }
            Order* node = head_;
            remove(node);
            return node;
        }
};