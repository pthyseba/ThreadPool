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

class TimedExecutorInterface;

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
    size_t Push(taCallable&& aCallable, int aExecutionTimeout = 0, int aId = 0)
    {
      std::unique_lock<std::mutex> l(iQueueMutex);	    
      bool wasEmpty = iWorkQueue.empty();
      iWorkQueue.push(new CallableWorkItem<taCallable>(std::forward<taCallable>(aCallable), aExecutionTimeout, aId));
      size_t id = iNextWorkItemId;
      iNextWorkItemId++;
      l.unlock();
      // Signal non-empty queue
      if (wasEmpty)
      {
        iQueueCv.notify_all();
      }

      return id;
    }

    void TryCancel(int aId); 
    
    void Stop();

  private:
    void workerMain(size_t aThreadId, std::atomic<int>* aCurrentWorkItem, std::atomic<TimedExecutorInterface*>* aTimedExecutor);
    
    bool iExit;
    size_t iNextWorkItemId;
    std::mutex iQueueMutex;
    std::condition_variable iQueueCv;
    std::queue<WorkItem*> iWorkQueue;
    std::vector<std::thread> iThreads;
    std::vector<std::atomic<int>*> iCurrentWorkItems;
    std::vector<std::atomic<TimedExecutorInterface*>*> iTimedExecutors; 
};

} // ThreadPool

#endif
