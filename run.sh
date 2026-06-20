#!/bin/bash

clang++ -std=c++20 -O3 -Wall -I include src/orderbook.cpp src/benchmark.cpp -o engine_benchmark

./engine_benchmark