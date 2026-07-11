#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

// spawning std::thread fresh every single tick turned out to be the actual
// bottleneck once i benchmarked on real hardware (windows thread creation
// is not cheap, way worse than linux apparently). this pool gets built
// once and workers just sit around waiting for jobs instead.
//
// usage: submit() a bunch of jobs, then waitAll() blocks until theyre all
// done. not trying to build a general purpose job system with futures and
// stealing and whatever, just enough to stop respawning threads every tick
class ThreadPool {
public:
    explicit ThreadPool(int numThreads) {
        if (numThreads < 1) numThreads = 1;
        for (int i = 0; i < numThreads; i++) {
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m);
            stop = true;
        }
        cvJob.notify_all();
        for (auto& t : workers) t.join();
    }

    void submit(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lk(m);
            jobs.push(std::move(job));
            pending++;
        }
        cvJob.notify_one();
    }

    void waitAll() {
        std::unique_lock<std::mutex> lk(m);
        cvDone.wait(lk, [this] { return pending == 0; });
    }

    int threadCount() const { return (int)workers.size(); }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m);
                cvJob.wait(lk, [this] { return stop || !jobs.empty(); });
                if (stop && jobs.empty()) return;
                job = std::move(jobs.front());
                jobs.pop();
            }
            job();
            {
                std::lock_guard<std::mutex> lk(m);
                pending--;
                if (pending == 0) cvDone.notify_all();
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    std::mutex m;
    std::condition_variable cvJob;
    std::condition_variable cvDone;
    int pending = 0;
    bool stop = false;
};