#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__

#include <condition_variable>
#include <functional>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sched.h>
#endif

namespace bsv
{

// Returns IDs of physical cores (one per physical core, excluding HT siblings).
// On Linux, reads /sys/devices/system/cpu/cpuN/topology/thread_siblings_list.
// Falls back to all logical cores on non-Linux platforms.
inline std::vector<int> getPhysicalCoreIds()
{
    std::vector<int> physical;
#ifdef __linux__
    std::set<int> seen;
    const int maxCpus = static_cast<int>(std::thread::hardware_concurrency());
    for (int cpu = 0; cpu < maxCpus; ++cpu) {
        std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(cpu)
                        + "/topology/thread_siblings_list");
        if (!f) continue;
        std::string line;
        std::getline(f, line);
        // stoi stops at first non-digit (comma or dash), giving the primary core id
        const int primary = std::stoi(line);
        if (seen.insert(primary).second)
            physical.push_back(primary);
    }
#else
    const int n = static_cast<int>(std::thread::hardware_concurrency());
    for (int i = 0; i < n; ++i)
        physical.push_back(i);
#endif
    return physical;
}

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads)
    {
        const auto physicalCores = getPhysicalCoreIds();
        const size_t numPhysical = physicalCores.size();

        std::cout << "[ThreadPool] Logical CPUs:      " << std::thread::hardware_concurrency() << "\n";
        std::cout << "[ThreadPool] Physical cores:    " << numPhysical << "\n";
        std::cout << "[ThreadPool] Threads to create: " << numThreads << "\n";

        workers.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            const int coreId = physicalCores[i % numPhysical];
            workers.emplace_back([this, coreId] {
#ifdef __linux__
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(coreId, &cpuset);
                if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
                    std::cerr << "[ThreadPool] Warning: sched_setaffinity failed for core "
                              << coreId << "\n";
                }
#endif
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
        std::cout << "[ThreadPool] All threads created and pinned to physical cores.\n";
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto& w : workers) {
            w.join();
        }
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F>
    auto submit(F&& f) -> std::future<decltype(f())>
    {
        using ReturnType = decltype(f());
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        auto future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.emplace([task]() { (*task)(); });
        }
        cv.notify_one();
        return future;
    }

    size_t size() const { return workers.size(); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;
};

} // namespace bsv

#endif /* __THREAD_POOL_HPP__ */
