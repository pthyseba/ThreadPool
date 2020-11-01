#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <setjmp.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "WorkItem.h"

namespace { 
  pid_t GetThreadId()
  {
    pid_t tid = syscall(SYS_gettid);
    return tid;
  }
}

namespace ThreadPool {

template<int taSignalNo = SIGUSR1>
class ConfigurableThreadPool
{
  private: 
    static void SetupHandler()
    {
      static std::atomic<bool> sHaveInitialized{false};
      bool expected = false;
      int result = sHaveInitialized.compare_exchange_strong(expected, true);
      if (result)
      {
	// Exactly once: setup timer expiry signal handler      
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = &Handler;
        sigaction(taSignalNo, &sa, nullptr);
      }
    }

    // Handler is shared between all ThreadPools using the same signal for timer events
    static void Handler(int aSignalNo, siginfo_t* aSigInfo, void* aContext)
    {
      // ASSUMPTION: this handler runs on the thread that needs to be interrupted
      if (aSignalNo == taSignalNo)
      {
        jmp_buf* buf = static_cast<jmp_buf*>(aSigInfo->si_value.sival_ptr);
        longjmp(*buf, 1);
      }
    } 
  
  public:
    ConfigurableThreadPool(size_t aThreadCount): iExit(false), iSignalNo(taSignalNo)
    {	    
      iEnv = new jmp_buf[aThreadCount];	    
      SetupHandler(); 

      // Launch worker threads
      iThreads.reserve(aThreadCount);	
      for(size_t i = 0; i < aThreadCount; ++i)
      {
	iThreads.emplace_back([this, i](){this->workerMain(i);});
      }
    }

    ~ConfigurableThreadPool()
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

      delete[] iEnv;
    }

    ConfigurableThreadPool(const ConfigurableThreadPool&) = delete;
    ConfigurableThreadPool& operator = (const ConfigurableThreadPool&) = delete;
    ConfigurableThreadPool(ConfigurableThreadPool&&) = delete;
    ConfigurableThreadPool& operator = (ConfigurableThreadPool&&) = delete;

    template<typename taCallable>
    void Push(taCallable&& aCallable, int aExecutionTimeout = 0)
    {
      std::unique_lock<std::mutex> l(iQueueMutex);	    
      iWorkQueue.push(new CallableWorkItem<taCallable>(std::forward<taCallable>(aCallable), aExecutionTimeout));
      l.unlock();
      // Signal non-empty queue
      iQueueCv.notify_all();
    }

    void Stop()
    {
      std::unique_lock<std::mutex> l(iQueueMutex);
      iExit = true;
      l.unlock();
      iQueueCv.notify_all();
    }

  private:
    void workerMain(size_t aThreadId)
    {
      try
      {	
	// Create timer used by this thread
        timer_t timerId;
	itimerspec timerConfig;
	sigevent sevp;
	sevp.sigev_signo = iSignalNo;
        // Direct signal to this thread
	sevp.sigev_notify = SIGEV_THREAD_ID;
        sevp._sigev_un._tid = GetThreadId();	
        // Ensure signal handler gets pointer to correct jmp_buf 
 	sevp.sigev_value.sival_ptr = static_cast<void*>(&iEnv[aThreadId]);
	const bool timeoutEnabled = (timer_create(CLOCK_REALTIME, &sevp, &timerId) == 0);
	
        if (!timeoutEnabled)
	{
          std::cerr << "Warning: timeout disabled for thread " << GetThreadId() << std::endl;
	}

	// Thread main loop
	for(;;) 
	{	
          std::unique_lock<std::mutex> l(iQueueMutex);
          iQueueCv.wait(l, [this](){return (!iWorkQueue.empty() || iExit) ;});
          // We have locked the mutex here; either there is work in the queue or exit has been requested 
	  if (iExit)
          {
            if (timeoutEnabled)
	    {
              timer_delete(timerId);
	    }
	    return;
	  }	

	  // Get next work item
	  const WorkItem* w = iWorkQueue.front();  
          iWorkQueue.pop();
	  // Unblock other worker threads
	  l.unlock();
	  if (w != nullptr)
	  {
	    try
	    { 
	      // Enable longjmp from timer signal handler to interrupt  
	      if (setjmp(iEnv[aThreadId]) == 0)	    
	      {
		const int timeoutMilliseconds = w->GetTimeoutInMilliseconds();      
                // Setup one-shot execution timer
		if (timeoutEnabled && (timeoutMilliseconds > 0))
		{
                  timerConfig.it_interval.tv_sec = 0;
		  timerConfig.it_interval.tv_nsec = 0;
	    	  timerConfig.it_value.tv_sec = timeoutMilliseconds / 1000;
	          timerConfig.it_value.tv_nsec = (timeoutMilliseconds % 1000) * 1000000;	
		  timer_settime(timerId, 0, &timerConfig, nullptr);
	        }

		// Actual work
		// NOTE: if timeout is enabled, do NOT allocate resources in operator()!
		w->Call();
                
		// Disable timer
		if (timeoutEnabled)
		{
		  timerConfig.it_value.tv_sec = 0;
		  timerConfig.it_value.tv_nsec = 0;
                  timer_settime(timerId, 0, &timerConfig, nullptr);
		}
	      }
	      else
	      { 
		// Timer fired during w->Call(), signal handler longjmp brings us back here
		std::cout << "Thread " << GetThreadId() << ": TIMEOUT" << std::endl;
	      }
	    }
	    catch(...)
	    { 
	       // Exeception thrown from w->Call(), disable timer
	       if (timeoutEnabled)
	       {
	          timerConfig.it_value.tv_sec = 0;
	          timerConfig.it_value.tv_nsec = 0;
	          timer_settime(timerId, 0, &timerConfig, nullptr);
	       }
	       std::cout << "Thread " << GetThreadId() << ": exception thrown from w->Call()..." << std::endl;
	    }

	    delete w;
	  }
	} // Thread main loop
      }
      catch(...)
      {
        std::cout << "Thread " << GetThreadId() << ": caught exception in main loop, exiting..." << std::endl;
      }
    }

    bool iExit;
    int iSignalNo;
    std::mutex iQueueMutex;
    std::condition_variable iQueueCv;
    std::queue<WorkItem*> iWorkQueue;
    std::vector<std::thread> iThreads;
    jmp_buf* iEnv;
};

typedef ConfigurableThreadPool<> ThreadPool;

} // ThreadPool

#endif
