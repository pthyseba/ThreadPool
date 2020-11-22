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
#include "ThreadPool.h"
#include "TimedExecutorInterface.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace ThreadPool {

ThreadPool::ThreadPool(size_t aThreadCount): iExit(false)
{	    
  iNextWorkItemId = 0;
  // Launch worker threads
  iThreads.reserve(aThreadCount);	
  iCurrentWorkItems.reserve(aThreadCount);
  for(size_t i = 0; i < aThreadCount; ++i)
  {
    iCurrentWorkItems.push_back(new std::atomic<int>(-1));
    iTimedExecutors.push_back(new std::atomic<TimedExecutorInterface*>(nullptr));
    iThreads.emplace_back([this, i](){this->workerMain(i, iCurrentWorkItems[i], iTimedExecutors[i]);});
  }
}

ThreadPool::~ThreadPool()
{
  Stop();

  for(auto& thread : iThreads)
  {
    thread.join();
  }

  while(!iWorkQueue.empty())
  {
    const WorkItem* w = iWorkQueue.front();
    iWorkQueue.pop();
    delete w;
  }

  for(size_t i = 0; i < iCurrentWorkItems.size(); ++i)
  {
    delete iCurrentWorkItems[i];
  }

  for(size_t i = 0; i < iTimedExecutors.size(); ++i)
  {
    delete iTimedExecutors[i];
  }
}

void ThreadPool::Stop()
{
  std::unique_lock<std::mutex> l(iQueueMutex);
  iExit = true;
  l.unlock();
  iQueueCv.notify_all();
}


void ThreadPool::TryCancel(int aId)
{
  // Prevent workers from taking up new work	
  std::unique_lock<std::mutex> l(iQueueMutex);
  // if aId is still in Queue
  // .. remove (need vector?)
  
  for (size_t i = 0; i < iCurrentWorkItems.size(); ++i)
  {
    if (iCurrentWorkItems[i]->load() == aId)
    {
      // Try to cancel running job
      // Need pointer to Thread-specific TimedExecutor
      if (iTimedExecutors[i]->load() != nullptr)
      {
        iTimedExecutors[i]->load()->TryCancel(aId);
      }
      return;
    }
  }
}

void ThreadPool::workerMain(size_t aThreadId, std::atomic<int>* aCurrentWorkItem, std::atomic<TimedExecutorInterface*>* aTimedExecutor)
{
  static auto console = spdlog::stdout_color_mt("console");
  try
  {
    // Create timer used by this thread
    TimedExecutor t(aCurrentWorkItem); 
    aTimedExecutor->store(&t); 
    // Thread main loop
    for(;;) 
    { 
      std::unique_lock<std::mutex> l(iQueueMutex);
      iQueueCv.wait(l, [this](){return (!iWorkQueue.empty() || iExit) ;});
      // We have locked the mutex here; either there is work in the queue or exit has been requested 
      console->info("Thread {}: Locked mutex", aThreadId);
      if (iExit)
      {
        console->info("Thread {}: Exiting", aThreadId);
	return;
      }	
      // Get next work item
      const WorkItem* w;
      w  = iWorkQueue.front();  
      iWorkQueue.pop();
      // Unblock other worker threads
      l.unlock();
      if (w != nullptr)
      {	
        try
        {	    
          t.ExecuteWithTimeout([w](){w->Call();}, w->GetTimeoutInMilliseconds(), w->GetId());
          console->info("Thread {}: FINISHED {}", aThreadId, w->GetId());
	}
        catch (const TimedExecutorInterface::TimeoutException& e)
        {
          console->info("Thread {}: TIMEOUT for {}", aThreadId, w->GetId());
        }
        catch(...)
        {
          console->info("Thread {}: EXCEPTION for {}", aThreadId, w->GetId());
        }
        delete w;
      }
    } // Thread main loop
  }
  // Win32 TimedExecutor bug?
  // workerkMain Mutex/cv wait throws exception in one thread  
  // if a non=generic catch is missing here
  catch(const char* e)
  {
    console->info("Thread {}: EXCEPTION in main loop: {},  exiting...", aThreadId, e);	  
  }
  catch(...)
  {
    console->info("Thread {}: EXCEPTION in main loop, exiting...", aThreadId);
  }
}

} // ThreadPool

