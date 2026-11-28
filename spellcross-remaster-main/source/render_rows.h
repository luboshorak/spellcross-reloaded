#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace spellcross_render_rows
{
    // One UI-thread-owned pool. All jobs are synchronous, immutable input,
    // disjoint output scanlines. Never call wx or game logic from a worker.
    class Pool
    {
        std::mutex mutex;
        std::condition_variable ready,finished;
        std::vector<std::thread> workers;
        std::function<void(int,int)> job;
        std::uint64_t generation=0;
        int height=0,remaining=0;
        bool stopping=false;
        void Worker(int index)
        {
            std::uint64_t seen=0;
            std::unique_lock<std::mutex> lock(mutex);
            for(;;)
            {
                ready.wait(lock,[&]{return stopping||generation!=seen;});
                if(stopping) return;
                seen=generation;
                const int n=static_cast<int>(workers.size())+1;
                const int begin=height*index/n,end=height*(index+1)/n;
                lock.unlock();
                job(begin,end);
                lock.lock();
                if(--remaining==0) finished.notify_one();
            }
        }
    public:
        Pool()
        {
            const unsigned n=std::clamp(std::thread::hardware_concurrency(),1u,8u);
            workers.reserve(n-1);
            for(unsigned i=1;i<n;++i)
            {
                try { workers.emplace_back([this,i]{Worker(static_cast<int>(i));}); }
                catch(...) { break; }
            }
        }
        ~Pool()
        {
            { std::lock_guard<std::mutex> lock(mutex); stopping=true; }
            ready.notify_all();
            for(auto& worker:workers) worker.join();
        }
        template<class Fn> void Run(int h,const Fn& fn)
        {
            if(workers.empty()||h<64) { fn(0,h); return; }
            const int n=static_cast<int>(workers.size())+1;
            {
                std::lock_guard<std::mutex> lock(mutex);
                job=fn; height=h; remaining=n-1; ++generation;
            }
            ready.notify_all();
            fn(0,h/n);
            std::unique_lock<std::mutex> lock(mutex);
            finished.wait(lock,[&]{return remaining==0;});
            job={};
        }
    };
    inline Pool& SharedPool() { static thread_local Pool pool; return pool; }
    template<class Fn> void Run(int height,bool parallel,const Fn& fn)
    {
        if(!parallel) { fn(0,height); return; }
        SharedPool().Run(height,fn);
    }
}
