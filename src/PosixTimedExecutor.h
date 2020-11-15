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
    
    PosixTimedExecutor() : iSignalNo(taSignalNo)
    {
      SetupHandler();
      CreateTimer();
    }

    virtual ~PosixTimedExecutor()
    {
      DeleteTimer();
    }
 
    virtual void ExecuteWithTimeout(TCallable&& aCallable, int aMilliseconds) override
    {
	if (aMilliseconds > 0)
	{
          if(!iTimerEnabled)
	  {
	    throw std::string("Timeout not enabled!");
	  }

	  try
	  {
            if (setjmp(iBuf) == 0)
	    {		    
              StartTimer(aMilliseconds);
	      aCallable();
	      StopTimer();
	    }
	    else // aCallable() potentially timed out
	    {  
              throw TimeoutException();
	    }
	  }
	  catch(const TimeoutException& e)
	  {
	    throw;	  
	  }
	  catch(...)
	  {
            StopTimer();
	    throw;
	  }
	}
	else // no timeout
	{ 
          try
	  {
            aCallable();
	  }
	  catch(...)
	  {
            throw;
	  }
	}

    } 

  private: 

    struct handlerData
    {
      int iSignalSource; // Timer vs abort 	    
      int iWorkId; // For abort
      jmp_buf* iBuf;
    };

    pid_t GetThreadId() const
    {
      pid_t tid = syscall(SYS_gettid);
      return tid;
    }

    void CreateTimer() 
    {
      sigevent sevp;
      memset(&sevp, 0, sizeof(sigevent));
      sevp.sigev_signo = iSignalNo;
      // Direct signal to this thread
      sevp.sigev_notify = SIGEV_THREAD_ID;
      sevp._sigev_un._tid = GetThreadId();
      // Ensure signal handler gets pointer to correct jmp_buf
      sevp.sigev_value.sival_ptr = static_cast<void*>(&iBuf);
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
        jmp_buf* buf = static_cast<jmp_buf*>(aSigInfo->si_value.sival_ptr);
        longjmp(*buf, 1);
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

    bool iTimerEnabled;
    int iSignalNo;
    timer_t iTimerId;
    jmp_buf iBuf;
    handlerData iTimerData;
    handlerData iAbortData;
};

#endif // __linux__

#endif
