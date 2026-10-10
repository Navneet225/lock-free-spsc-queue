// Lock-free SPSC ring buffer (C++17) vs mutex-based queue benchmark.
// Build: g++ -O2 -std=c++17 -pthread spsc_queue.cpp -o spsc_queue
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Single-producer / single-consumer lock-free ring buffer.
// Capacity must be a power of two. Uses acquire/release ordering only.
template <typename T, size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    bool push(const T& v) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) return false;  // full
        buf_[head] = v;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;  // empty
        out = buf_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

private:
    static constexpr size_t kMask = Capacity - 1;
    // Separate cache lines for producer and consumer indices (avoid false sharing).
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::vector<T> buf_ = std::vector<T>(Capacity);
};

// Baseline: mutex + condition_variable bounded queue.
template <typename T>
class MutexQueue {
public:
    explicit MutexQueue(size_t cap) : cap_(cap) {}
    void push(const T& v) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_; });
        q_.push(v);
        not_empty_.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty(); });
        T v = q_.front();
        q_.pop();
        not_full_.notify_one();
        return v;
    }

private:
    std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    std::queue<T> q_;
    size_t cap_;
};

int main() {
    constexpr uint64_t N = 5'000'000;
    using clk = std::chrono::steady_clock;

    // ---- Lock-free SPSC ----
    SpscQueue<uint64_t, 1024> q;
    uint64_t sum_lf = 0;
    auto t0 = clk::now();
    std::thread prod([&] {
        for (uint64_t i = 1; i <= N; ++i)
            while (!q.push(i)) std::this_thread::yield();
    });
    std::thread cons([&] {
        uint64_t v, got = 0;
        while (got < N) {
            if (q.pop(v)) { sum_lf += v; ++got; }
            else std::this_thread::yield();
        }
    });
    prod.join(); cons.join();
    double lf_s = std::chrono::duration<double>(clk::now() - t0).count();

    // ---- Mutex baseline ----
    MutexQueue<uint64_t> mq(1024);
    uint64_t sum_mx = 0;
    t0 = clk::now();
    std::thread prod2([&] { for (uint64_t i = 1; i <= N; ++i) mq.push(i); });
    std::thread cons2([&] { for (uint64_t i = 0; i < N; ++i) sum_mx += mq.pop(); });
    prod2.join(); cons2.join();
    double mx_s = std::chrono::duration<double>(clk::now() - t0).count();

    const uint64_t expected = N * (N + 1) / 2;
    std::printf("Items transferred : %llu\n", (unsigned long long)N);
    std::printf("Correctness       : lock-free=%s mutex=%s\n",
                sum_lf == expected ? "OK" : "FAIL", sum_mx == expected ? "OK" : "FAIL");
    std::printf("Lock-free SPSC    : %.3f s  (%.2f M ops/s)\n", lf_s, N / lf_s / 1e6);
    std::printf("Mutex+CV queue    : %.3f s  (%.2f M ops/s)\n", mx_s, N / mx_s / 1e6);
    std::printf("Speedup           : %.2fx\n", mx_s / lf_s);
    return 0;
}
