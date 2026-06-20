#include <iostream>
#include <cstring>
#include <cstddef>
#include "order.hpp"
#include "objectpool.hpp"

struct PriceLevel{

    double price;
    int total_volume;
    Order* head;
    Order* tail;

    void reset()
    {
        price=0;
        total_volume=0;
        head=nullptr;
        tail=nullptr;
    }

};

class OrderBook{
private:
    ObjectPool<Order, 5500000> order_pool;
    static constexpr size_t NUM_SLOTS = 501;
    static constexpr double MIN_PRICE = 100.0;
    static constexpr double TICK_SIZE = 0.01;

    PriceLevel bid_levels[NUM_SLOTS];
    PriceLevel ask_levels[NUM_SLOTS];

    std::ptrdiff_t best_bid_idx;
    std::ptrdiff_t best_ask_idx;

    static constexpr size_t MAX_ORDERS = 5500000;
    Order* order_directory[MAX_ORDERS];

    inline size_t price_to_index(double price) const
    {
        return static_cast<size_t>((price-MIN_PRICE)/TICK_SIZE);
    }
public:
    OrderBook()
    : best_bid_idx(-1), best_ask_idx(-1)
    {
        for(size_t i=0;i<NUM_SLOTS;++i)
        {
            bid_levels[i].reset();
            ask_levels[i].reset();
        }
        std::memset(order_directory,0,sizeof(order_directory));
    }

    ~OrderBook() = default;
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    void add_order(int id, Ticker ticker, double price, int volume, bool is_buy, bool is_market, bool is_ioc);
    void cancel_order(int id);

    void print_bbo() const {
        double bid = (best_bid_idx != -1) ? bid_levels[best_bid_idx].price : 0.0;
        double ask = (best_ask_idx != -1) ? ask_levels[best_ask_idx].price : 0.0;
        std::cout << "BBO: " << bid << " | " << ask << std::endl;
    }
    
};