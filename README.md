# C++ Limit Order Book (LOB)

A matching engine written in C++20 designed to explore low-latency systems engineering. The primary goal of this project is to process market data (Limit Adds, Cancels, Market Sweeps, and IOCs) with zero dynamic memory allocations on the critical path.

## About the Project

In performance-critical applications, the biggest enemy of speed is the operating system. Every time a program uses standard C++ features like `new`, `malloc`, or `std::map` to create an object, the OS pauses the program to find and allocate memory. 

This project bypasses the standard OS heap manager. By pre-allocating a massive block of memory at startup, managing pointers manually, and packing data tightly to fit inside the CPU's hardware cache, this engine proves how to process millions of orders in nanoseconds without ever asking the OS for memory during a trade.

## Architecture

The engine relies on three core components to achieve zero-allocation execution.

### 1. Direct-Mapped Price Arrays
Instead of using a Hash Map or a Binary Tree to find where a price belongs, the engine uses a flat, fixed-size array. We take the incoming price, use simple math to turn it into an array index, and jump straight to that memory slot in a single CPU cycle ($O(1)$ time).

```text
Incoming Price: $100.02
Math: (100.02 - 100.00) / 0.01 = Index 2

Array Memory Block:
[Index 0: $100.00] -> [ Order A <-> Order B ]
[Index 1: $100.01] -> [ Empty ]
[Index 2: $100.02] -> [ Order C ]  <-- Jump directly here
```

### 2. Intrusive Linked Lists
Standard C++ lists (`std::list`) create a "Node" that points to your data. This means allocating memory twice (once for the Node, once for the Data), scattering them across RAM. This engine uses an *intrusive* list: the `Order` structure itself contains the `next` and `prev` memory pointers.

```text
Standard List:  [ Node Pointer ] ----> (RAM Gap) ----> [ Order Data ]
Intrusive List: [ Order Data | Next Ptr | Prev Ptr ]   (All in one cache line)
```

### 3. Pre-Faulted Object Pool
When the program starts, it asks the OS for a massive chunk of raw memory using `mmap`. It then loops through and writes a `0` to every page of that memory. This forces the physical RAM to map to the program *before* trading starts, preventing the OS from interrupting the program later to handle "page faults."

```text
1. Startup: Grab 44MB of RAM -> Write zeros to force physical mapping.
2. Trading: Grab an empty Order slot -> Fill with data -> Link to Price Array.
3. Match:   Unlink Order -> Erase Data -> Return slot to the Pool.
```

## Benchmark Results

The benchmark simulates a burst of 5,000,000 randomized orders, measured on an Apple M-series processor (macOS). I/O and data generation are separated from the timed execution loop. A 100,000-order warm-up phase runs first to prime the instruction cache. 

*Compiled via Apple Clang (`-O3 -std=c++20`).*

* **Throughput:** ~24.8 million msgs/sec
* **Mean Latency:** 26 ns
* **p99 Latency:** 125 ns 
* **p99.9 (Tail) Latency:** 2000 ns 

*Note: The maximum latency occasionally spikes to ~1.5ms. This is because macOS is a desktop OS that randomly pauses programs to run background tasks. True hardware limits require a dedicated Linux kernel.*

## Limitations

1. **Memory Waste:** The flat price array allocates memory for every possible price tick between the minimum and maximum bounds. If no one trades at a specific price, that memory just sits empty.
2. **Fixed Capacity:** The system can only hold 5.5 million active orders. Because we refuse to ask the OS for more memory while running, if the pool fills up, the engine will drop new orders.
3. **Clean Data Assumption:** Real stock exchanges send messy, complex network streams. This benchmark assumes the data has already been perfectly formatted into clean structs before reaching the engine.

## Future Improvements

* **Multithreading:** One thread should constantly listen to the network for new orders, while a second thread strictly handles the matching logic. They would communicate using a lock-free ring buffer.
* **Smart Memory Allocation:** Upgrading the flat array to a sparse mapping system would allow the engine to only use memory for prices where actual trades are happening.
* **Linux Tuning:** Moving the code to a Linux environment would allow us to lock the engine to a single CPU core, completely stopping the OS from interrupting the benchmark.
