#include "orderbook.hpp"
#include <iostream>
#include <algorithm>

void OrderBook::add_order(int id, Ticker ticker, double price, int volume, bool is_buy, bool is_market, bool is_ioc)
{
    size_t idx = price_to_index(price);
    int remaining_volume = volume;
    if(is_buy)
    {
        while(remaining_volume>0 && best_ask_idx!=-1 && 
            (is_market || price>=ask_levels[best_ask_idx].price))
        {
            PriceLevel& level = ask_levels[best_ask_idx];
            Order* current = level.head;
            while(current && remaining_volume>0)
            {
                Order* next = current->next_order;
                int match_volume = std::min(remaining_volume,current->volume);
                remaining_volume -= match_volume;
                current->volume -= match_volume;
                level.total_volume -= match_volume;

                if(current->volume==0)
                {
                    level.head = next;
                    if(level.head) level.head->prev_order = nullptr;
                    else level.tail = nullptr;

                    order_directory[current->id] = nullptr;
                    order_pool.deallocate(current);
                }
                current = next;
            }
            if(!level.head)
            {
                level.reset();
                std::ptrdiff_t next_idx = best_ask_idx + 1;
                while(next_idx < static_cast<std::ptrdiff_t>(NUM_SLOTS) && !ask_levels[next_idx].head) 
                {
                    next_idx++;
                }
                best_ask_idx = (next_idx < static_cast<std::ptrdiff_t>(NUM_SLOTS)) ? next_idx : -1;
            }
        }
    }
    else
    {
        while(remaining_volume>0 && best_bid_idx!=-1 && 
            (is_market || price <= bid_levels[best_bid_idx].price))
        {
            PriceLevel& level = bid_levels[best_bid_idx];
            Order* current = level.head;
            while(current && remaining_volume > 0)
            {
                Order* next = current->next_order;
                int match_volume = std::min(remaining_volume,current->volume);
                remaining_volume -= match_volume;
                current->volume -= match_volume;
                level.total_volume -= match_volume;

                if(current->volume == 0)
                {
                    level.head = next;
                    if(level.head) level.head->prev_order = nullptr;
                    else level.tail = nullptr;

                    order_directory[current->id] = nullptr;
                    order_pool.deallocate(current);
                }
                current = next;
            }
            if(!level.head)
            {
                level.reset();
                std::ptrdiff_t next_idx = best_bid_idx - 1;
                while(next_idx > 0 && (!bid_levels[next_idx].head))
                {
                    --next_idx;
                }
                best_bid_idx = next_idx;
            }
        }
    }
    if(remaining_volume > 0)
    {
        if(is_market || is_ioc) return;

        Order* new_order = order_pool.allocate(ticker,price,id,remaining_volume,is_buy);
        if(!new_order) return;

        order_directory[id] = new_order;
        PriceLevel& level = is_buy ? bid_levels[idx] : ask_levels[idx];

        if(!level.head)
        {
            level.price = price;
            level.head = new_order;
            level.tail = new_order;
        }
        else
        {
            level.tail->next_order = new_order;
            new_order->prev_order = level.tail;
            level.tail = new_order;
        }
        level.total_volume += new_order->volume;
        std::ptrdiff_t signed_idx = static_cast<std::ptrdiff_t>(idx);
        if(is_buy)
        {
            if(best_bid_idx == -1 || signed_idx > best_bid_idx) best_bid_idx = signed_idx;
        }
        else
        {
            if(best_ask_idx == -1 || signed_idx < best_ask_idx ) best_ask_idx = signed_idx;
        }
    }
}

void OrderBook::cancel_order(int id)
{
    Order* order = order_directory[id];
    if(!order) return;

    size_t idx = price_to_index(order->price);
    PriceLevel& level = order->isbuy ? bid_levels[idx] : ask_levels[idx];
    if(order->prev_order)
    {
        order->prev_order->next_order = order->next_order;
    }
    else
    {
        level.head = order->next_order;
    }
    if(order->next_order)
    {
        order->next_order->prev_order = order->prev_order;
    }
    else
    {
        level.tail = order->prev_order;
    }
    level.total_volume -= order->volume;
    order_directory[id] = nullptr;
    order_pool.deallocate(order);

    if(!level.head)
    {
        level.reset();
        std::ptrdiff_t signed_idx = static_cast<std::ptrdiff_t>(idx);

        if (order->isbuy && signed_idx == best_bid_idx) {
            std::ptrdiff_t next_idx = best_bid_idx - 1;
            while (next_idx >= 0 && !bid_levels[next_idx].head) {
                --next_idx;
            }
            best_bid_idx = next_idx;
        } 
        else if (!order->isbuy && signed_idx == best_ask_idx) {
            std::ptrdiff_t next_idx = best_ask_idx + 1;
            while (next_idx < static_cast<std::ptrdiff_t>(NUM_SLOTS) && !ask_levels[next_idx].head) {
                ++next_idx;
            }
            best_ask_idx = (next_idx < static_cast<std::ptrdiff_t>(NUM_SLOTS)) ? next_idx : -1;
        }
    }
}