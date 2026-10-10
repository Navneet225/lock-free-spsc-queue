# Lock-Free SPSC Queue (C++17)

Templated single-producer/single-consumer ring buffer built on `std::atomic`
with acquire/release memory ordering. Producer and consumer indices sit on
separate cache lines (`alignas(64)`) to avoid false sharing.

The benchmark transfers 5M items and compares it with a mutex + condition-variable
queue, checking correctness with a checksum.

```bash
make && ./spsc_queue
```

Sample run (1-vCPU sandbox): lock-free ~262M ops/s vs mutex queue ~12.5M ops/s (~21x).
Numbers depend on hardware and core count, so run it on your own machine.

Requirements: Linux, g++ with C++17.
