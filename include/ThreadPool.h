#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "WorkItem.h"

namespace ThreadPool {

class ThreadPool
{
  public:
    ThreadPool(size_t aThreadCount);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator = (const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator = (ThreadPool&&) = delete;

    template<typename taCallable>
    void Push(taCallable&& aCallable, int aExecutionTimeout = 0, std::string aId = std::string(""))
    {
      std::unique_lock<std::mutex> l(iQueueMutex);	    
      bool wasEmpty = iWorkQueue.empty();
      iWorkQueue.push(new CallableWorkItem<taCallable>(std::forward<taCallable>(aCallable), aExecutionTimeout, aId));
      l.unlock();
      // Signal non-empty queue
      if (wasEmpty)
      {
        iQueueCv.notify_all();
      }
    }

    void Stop();

  private:
    void workerMain(size_t aThreadId);
    
    bool iExit;
    std::mutex iQueueMutex;
    std::condition_variable iQueueCv;
    std::queue<WorkItem*> iWorkQueue;
    std::vector<std::thread> iThreads;
};

} // ThreadPool

#endif
