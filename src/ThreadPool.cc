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
  // Launch worker threads
  iThreads.reserve(aThreadCount);	
  for(size_t i = 0; i < aThreadCount; ++i)
  {
    iThreads.emplace_back([this, i](){this->workerMain(i);});
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
}

void ThreadPool::Stop()
{
  std::unique_lock<std::mutex> l(iQueueMutex);
  iExit = true;
  l.unlock();
  iQueueCv.notify_all();
}

void ThreadPool::workerMain(size_t aThreadId)
{
  static auto console = spdlog::stdout_color_mt("console");
  try
  {
    // Create timer used by this thread
    TimedExecutor t; 
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
          t.ExecuteWithTimeout([w](){w->Call();}, w->GetTimeoutInMilliseconds());
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

