#include<iostream>
#include<cstring>
#include<new>
#include<utility>
#include "objectpool.hpp"


template<size_t N>
class FixedString
{
    public:
    FixedString(){
        std::memset(m_data,0,N);
    };

    FixedString(const char* str)
    {
        std::memset(m_data,0,N);
        std::memcpy(m_data,str,N-1);
    }

    bool operator==(const FixedString other) const
    {
        return std::memcmp(m_data,other.m_data,N)==0;
    }
    private:
    char m_data[N];
};

using Ticker =  FixedString<8>;

enum class MsgType{ADD, CANCEL};

struct Order{

    Ticker ticker;
    int id;
    double price;
    int volume;
    bool isbuy;
    Order* next_order;
    Order* prev_order;
    Order(Ticker ticker, double price, int id, int volume, bool buy) :
    ticker(ticker), id(id), price(price), volume(volume), isbuy(buy), next_order(nullptr), prev_order(nullptr){}
};


