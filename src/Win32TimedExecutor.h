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
    Win32TimedExecutor(std::atomic<int>* aWorkItem)
      : TimedExecutorInterface(aWorkItem), iInterruptible(false) 
    {
      // Note that simply calling GetCurrentThread() returns a handle
      // which is only meamingful in the calling thread...
      iThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
      iTimerData.iThread = iThread;
      iTimerData.iCurrentWorkItem = iCurrentWorkItem;
      iTimerData.iInterruptible = &iInterruptible;
      iTimerData.iBuf = &iBuf;
      iAbortData.iThread = iThread;
      iAbortData.iCurrentWorkItem = iCurrentWorkItem;
      iAbortData.iInterruptible = &iInterruptible;
      iAbortData.iBuf = &iBuf;
      // Register setjmp buffer for this thread
      TimeoutReturn(&iTimerData);
      AbortReturn(&iAbortData);
    }

    virtual ~Win32TimedExecutor() 
    {
      StopTimer();      
    }

    void TryCancel(int aItem) override
    {
      iAbortData.iExpectedWorkItem = aItem; 
      SetupThreadContextForReturn(&iAbortData, &AbortReturn);
    }

    void ExecuteWithTimeout(TCallable&& aCallable, int aMilliseconds, int aItem) override
    {
      try
      {
        iCurrentWorkItem->store(aItem);
	iTimerData.iExpectedWorkItem = aItem;
	// http://www.agardner.me/golang/windows/cgo/64-bit/setjmp/longjmp/2016/02/29/go-windows-setjmp-x86.html
        if (__builtin_setjmp(iBuf) == 0) 
        {
	  iInterruptible.store(true);
          if (aMilliseconds > 0)
	  {
            StartTimer(aMilliseconds);
	  }
	  aCallable();
	  iInterruptible.store(false);
          iCurrentWorkItem->store(-1);
	  if (aMilliseconds > 0)
	  {	  
	    StopTimer();
	  }
        }
        else
        {
	  // Either timed out or RetyCancel called.
	  // Try to disable timer anyway in catch block.
	  iInterruptible.store(false);
          throw TimeoutException();
        }
      }
      catch(...)
      {
	iInterruptible.store(false);
	iCurrentWorkItem->store(-1);      
        if (aMilliseconds > 0)
	{
	  StopTimer();
	}
	throw;
      }
    }

  private:
    struct handlerData
    {
      HANDLE iThread;
      std::atomic<int>* iCurrentWorkItem;
      int iExpectedWorkItem;
      std::atomic<bool>* iInterruptible;
      jmp_buf* iBuf;
    };

    void StartTimer(int aMilliseconds)
    {
      iTimerEnabled = CreateTimerQueueTimer(&iTimer, NULL, WaitOrTimerCallback, &iTimerData, aMilliseconds, 0, 0);
    }

    void StopTimer() 
    {
      if (iTimerEnabled) 
      {	      
        DeleteTimerQueueTimer(NULL, iTimer, NULL);
	iTimerEnabled = false;
      }
    }

    static void SetupThreadContextForReturn(handlerData* data, void (*returnFunc)(handlerData*))
    {
      HANDLE hThread = data->iThread;
      DWORD dwVal = SuspendThread(hThread);
      if (INFINITE != dwVal)
      {
        // Check whether thread is in interruptible state and qhether expected equals actual work item
        if (!data->iInterruptible->load())
	{
          ResumeThread(hThread);
	  return;
	}

	if (data->iExpectedWorkItem != data->iCurrentWorkItem->load())
	{
          ResumeThread(hThread);
	  return;
	}

    	// Get its context (processor registers)
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(hThread, &ctx))
        {
          // Jump into our TimeoutReturn() function
          ctx.Rip = (DWORD) (DWORD_PTR) returnFunc;
          SetThreadContext(hThread, &ctx);
        }
        // go ahead
        ResumeThread(hThread);
      }
    }

    static VOID CALLBACK WaitOrTimerCallback(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired)
    {
      // https://www.codeproject.com/Articles/71529/Exception-Injection-Throwing-an-Exception-in-Other
      handlerData* data = static_cast<handlerData*>(lpParameter);
      SetupThreadContextForReturn(data, &TimeoutReturn);
    }

    static void TimeoutReturn(handlerData* aData) 
    {
       static thread_local handlerData* data = nullptr;

       if (data == nullptr)
       {
         data = aData;
       }
       else
       {
	  __builtin_longjmp(*(data->iBuf), 1);
       }      
    }

    static void AbortReturn(handlerData* aData)
    {
      static thread_local handlerData* data = nullptr;

      if (data == nullptr)
      {
        data = aData;
      }
      else
      {
        __builtin_longjmp(*(data->iBuf), 1);
      }
    }

    bool iTimerEnabled;
    std::atomic<bool> iInterruptible;
    HANDLE iThread;
    HANDLE iTimer;
    jmp_buf iBuf;
    handlerData iTimerData;
    handlerData iAbortData;
};


#endif
