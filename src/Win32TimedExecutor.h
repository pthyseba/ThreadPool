#ifndef WIN32TIMEDEXECUTOR_H
#define WIN32TIMEDEXECUTOR_H

#include "processthreadsapi.h"
#include "threadpoollegacyapiset.h"
#include <windows.h>
#include <process.h>
#include "TimedExecutorInterface.h"
#include <setjmp.h>

class Win32TimedExecutor : public TimedExecutorInterface
{
  public:
    Win32TimedExecutor() 
    {
      // Note that simply calling GetCurrentThread() returns a handle
      // which is only meamingful in the calling thread...
      iThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
      // Register setjmp buffer for this thread
      TimeoutReturn(&iBuf); 
    }

    virtual ~Win32TimedExecutor() 
    {
      StopTimer();      
    }

    void ExecuteWithTimeout(TCallable&& aCallable, int aMilliseconds) override
    {
      if (aMilliseconds > 0)
      {
        try
        {
	  // http://www.agardner.me/golang/windows/cgo/64-bit/setjmp/longjmp/2016/02/29/go-windows-setjmp-x86.html
	  if (__builtin_setjmp(iBuf) == 0) 
	  {
	    StartTimer(aMilliseconds);
            aCallable();
            StopTimer();
	  }
	  else
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
    void StartTimer(int aMilliseconds)
    {
      iTimerEnabled = CreateTimerQueueTimer(&iTimer, NULL, WaitOrTimerCallback, &iThread, aMilliseconds, 0, 0);
    }

    void StopTimer() 
    {
      if (iTimerEnabled) 
      {	      
        DeleteTimerQueueTimer(NULL, iTimer, NULL);
	iTimerEnabled = false;
      }
    }

    static VOID CALLBACK WaitOrTimerCallback(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired)
    {
      // https://www.codeproject.com/Articles/71529/Exception-Injection-Throwing-an-Exception-in-Other
      HANDLE* hThread = static_cast<HANDLE*>(lpParameter);  
      // Suspend the thread, so that we won't have surprises
      DWORD dwVal = SuspendThread(*hThread);
      if (INFINITE != dwVal)
      {
	// Get its context (processor registers)
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_CONTROL;
      	if (GetThreadContext(*hThread, &ctx))
        {
          // Jump into our TimeoutReturn() function
          ctx.Rip = (DWORD) (DWORD_PTR) &TimeoutReturn;
          SetThreadContext(*hThread, &ctx);
        }
        // go ahead
        ResumeThread(*hThread);
      }	    
    }

    static void TimeoutReturn(jmp_buf* aBuf) 
    {
       static thread_local jmp_buf* buf = nullptr;

       if (buf == nullptr)
       {
         buf = aBuf;
       }
       else
       {
	 __builtin_longjmp(*buf, 1);
       }      
    }

    bool iTimerEnabled;
    HANDLE iThread;
    HANDLE iTimer;
    jmp_buf iBuf;
};


#endif
