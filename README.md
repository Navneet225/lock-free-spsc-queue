# Lock-Free SPSC Queue (C++17)

A templated, fixed-capacity, lock-free single-producer / single-consumer (SPSC) ring buffer, benchmarked against a classic mutex + condition-variable queue.

## Features
- Header-style template: `SpscQueue<T, Capacity>`
- Lock-free and wait-free for the single producer and single consumer: no mutexes, no syscalls on the hot path
- Only `std::memory_order_acquire` / `release` (plus `relaxed` for the owner's own index), no sequentially consistent fences
- `head` and `tail` indices on separate cache lines (`alignas(64)`) to avoid false sharing
- Power-of-two capacity, so wrap-around is a bitmask instead of a modulo
- Built-in benchmark with a correctness check (checksum of all transferred values)

## How it works
The producer owns `head_`, the consumer owns `tail_`.
- `push`: reads its own `head_` (relaxed), checks `tail_` (acquire) to see if the buffer is full, writes the element, then publishes it by storing the new `head_` (release).
- `pop`: reads its own `tail_` (relaxed), checks `head_` (acquire) to see if the buffer is empty, reads the element, then frees the slot by storing the new `tail_` (release).

The release store on one side pairs with the acquire load on the other, which guarantees the consumer never sees an index before the data written to that slot. One slot is kept empty to tell "full" from "empty", so usable capacity is `Capacity - 1`.

## Build and run
Requirements: Linux, g++ with C++17.

```bash
make
./spsc_queue
```

On Windows, run this inside WSL.

## Benchmark
Transfers 5,000,000 `uint64_t` values from one thread to another through a queue of 1024 slots, for both implementations, and verifies the sum of all received values.

Sample output:
```
Items transferred : 5000000
Correctness       : lock-free=OK mutex=OK
Lock-free SPSC    : 0.019 s  (262.57 M ops/s)
Mutex+CV queue    : 0.399 s  (12.53 M ops/s)
Speedup           : 20.96x
```

This run was on a single-vCPU sandbox, so both threads shared one core. Treat the exact numbers as indicative: results vary with core count, CPU, and thread placement. Run it on your own machine for your own numbers.

## Limitations
- Strictly one producer thread and one consumer thread
- Fixed capacity, which must be a power of two
- Elements are copied (`T` must be copy-assignable); no move or emplace API yet
- The benchmark uses `std::this_thread::yield()` when the queue is full or empty rather than busy-spinning

## Possible improvements
- Move semantics / `emplace`
- Cached copies of the opposite index to reduce cache-line traffic
- Pin threads to cores with `pthread_setaffinity_np` and benchmark per-operation latency, not just throughput
- MPMC variant

## Author
Navneet Singh Yadav - [LinkedIn](https://www.linkedin.com/in/navneet-singh-yadav-439886281/)
