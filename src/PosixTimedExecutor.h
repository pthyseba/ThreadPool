#ifndef POSIXTIMEDEXECUTOR_H
#define POSIXTIMEDEXECUTOR_H

#ifdef __linux__

#include <atomic>
#include <cstring>
#include <signal.h>
#include <string>
#include <sys/syscall.h>
#include <time.h>
#include <setjmp.h>
#include <unistd.h>

#include "TimedExecutorInterface.h"

template<int taSignalNo = SIGUSR1>
class PosixTimedExecutor : public TimedExecutorInterface
{
  public:
    
    PosixTimedExecutor(std::atomic<int>* aWorkItem) 
      : iSignalNo(taSignalNo), iCurrentWorkItem(aWorkItem), iInterruptible(false), iHandlerActive(false),
	iThreadId(GetThreadId()), iPthread_t(pthread_self())
    {
      iCurrentWorkItem->store(-1);	    
      SetupInterruptData();
      SetupHandler();
      CreateTimer();
    }

    virtual ~PosixTimedExecutor()
    {
      DeleteTimer();
    }
 
    // Only function to be called from outside a ThreadPool worker thread
    virtual void TryCancel(int aId) override
    {
      // Either returns ESRCH or SIGSEGV...
      /*	    
      iAbortData.iExpectedWorkItem = aId;      
      sigval v;
      v.sival_ptr = &iAbortData;
      int result = pthread_sigqueue(iThreadId, iSignalNo, v);
      switch (result)
      {
        case EAGAIN:
		break;
	case EINVAL:
		break;
	case ESRCH:
		break;
      }
      */
      StartTimer(1);  	    
    }

    virtual void ExecuteWithTimeout(TCallable&& aCallable,  int aMilliseconds, int aWorkItem) override
    {
	// Note: to be interruptible, jmp_buf must be used even in absence of timeout
        iCurrentWorkItem->store(aWorkItem);     
	iTimerData.iExpectedWorkItem = aWorkItem;
	bool useTimeout = iTimerEnabled && (aMilliseconds > 0);
	try 
	{
          if (setjmp(iBuf) == 0)
	  {
            iInterruptible.store(true);
	    if (useTimeout)
	    {
	      StartTimer(aMilliseconds);
	    }
	    aCallable();
	    iInterruptible.store(false);
	    iCurrentWorkItem->store(-1);
	    if (useTimeout)
	    {
	      StopTimer();
	    }
	  }
	  else
	  {
	    // We got here by longjmp, meaning the signal handler did not return.
	    // As a result, the timer signal is still blocked here.	  
            // Unblocking timer signal below.
	    sigset_t s;
	    sigemptyset(&s);
	    sigaddset(&s, iSignalNo);
	    pthread_sigmask(SIG_UNBLOCK, &s, NULL);		  
	    iHandlerActive.store(false);		  
            throw TimeoutException();
	  }
	}
	catch(...)
	{
          iInterruptible.store(false);
	  iCurrentWorkItem->store(-1);
	  iHandlerActive.store(false);
	  if (useTimeout)
	  {  
            StopTimer();
	  }
	  throw;
	}
    } 

  private: 

    struct handlerData
    {
      int iSignalSource; // Timer vs abort 	    
      int iExpectedWorkItem; // For abort
      std::atomic<int>* iCurrentWorkItem;
      std::atomic<bool>* iHandlerActive;
      std::atomic<bool>* iInterruptible;
      jmp_buf* iBuf;
    };

    pid_t GetThreadId() const
    {
      pid_t tid = syscall(SYS_gettid);
      return tid;
    }

    void SetupInterruptData()
    {
      iAbortData.iBuf = &iBuf;
      iAbortData.iHandlerActive = &iHandlerActive;
      iAbortData.iInterruptible = &iInterruptible;
      iAbortData.iCurrentWorkItem = iCurrentWorkItem;
      iTimerData.iBuf = &iBuf;
      iTimerData.iHandlerActive = &iHandlerActive;
      iTimerData.iInterruptible = &iInterruptible;
      iTimerData.iCurrentWorkItem = iCurrentWorkItem;
    }

    void CreateTimer() 
    {
      sigevent sevp;
      memset(&sevp, 0, sizeof(sigevent));
      sevp.sigev_signo = iSignalNo;
      // Direct signal to this thread
      sevp.sigev_notify = SIGEV_THREAD_ID;
      //iThreadId = GetThreadId();
      //iPthread_t = pthread_self();
      sevp._sigev_un._tid = iThreadId;
      // Ensure signal handler gets pointer to correct jmp_buf
      sevp.sigev_value.sival_ptr = static_cast<void*>(&iTimerData);
      iTimerEnabled = (timer_create(CLOCK_REALTIME, &sevp, &iTimerId) == 0);
    }

    void DeleteTimer()
    {
      if(iTimerEnabled)
      {
        timer_delete(iTimerId);
      }
    }

    void StartTimer(int aMilliseconds) const 
    {
      if(!iTimerEnabled)
      {
        return;
      }

      itimerspec timerConfig;
      timerConfig.it_interval.tv_sec = 0;
      timerConfig.it_interval.tv_nsec = 0;
      timerConfig.it_value.tv_sec = aMilliseconds / 1000;
      timerConfig.it_value.tv_nsec = (aMilliseconds % 1000) * 1000000;
      timer_settime(iTimerId, 0, &timerConfig, nullptr);
    }

    void StopTimer() const 
    {
      if(!iTimerEnabled)
      {
	return;
      } 

      itimerspec timerConfig;
      timerConfig.it_interval.tv_sec = 0;
      timerConfig.it_interval.tv_nsec = 0;
      timerConfig.it_value.tv_sec = 0;
      timerConfig.it_value.tv_nsec = 0;
      timer_settime(iTimerId, 0, &timerConfig, nullptr);
    }

    static void Handler(int aSignalNo, siginfo_t* aSigInfo, void* aContext)
    {
      // ASSUMPTION: this handler runs on the thread that needs to be interrupted
      if (aSignalNo == taSignalNo)
      {
	handlerData* data = static_cast<handlerData*>(aSigInfo->si_value.sival_ptr); 
	// Check whether we were interrupting another handler on this TimedExecutor
	bool expected = false;
	if (!data->iHandlerActive->compare_exchange_strong(expected, true))
        {
          return;
	}

	// Check interruptible status of this thread
        if (!data->iInterruptible->load())
	{
          data->iHandlerActive->store(false);		
          return;
	}

	// Check active work item is the expected work item
        if (data->iExpectedWorkItem != data->iCurrentWorkItem->load())
	{
          data->iHandlerActive->store(false);		
	  return;
	}	
	
	longjmp(*(data->iBuf), 1);
	// ORIGINAL
	//jmp_buf* buf = static_cast<jmp_buf*>(aSigInfo->si_value.sival_ptr);
        //longjmp(*buf, 1);
      }
    }
    
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
    };

    pid_t iThreadId;
    pthread_t iPthread_t;
    std::atomic<bool> iInterruptible;
    std::atomic<bool> iHandlerActive;
    std::atomic<int>* iCurrentWorkItem;
    bool iTimerEnabled;
    int iSignalNo;
    timer_t iTimerId;
    jmp_buf iBuf;
    handlerData iTimerData;
    handlerData iAbortData;
};

#endif // __linux__

#endif
