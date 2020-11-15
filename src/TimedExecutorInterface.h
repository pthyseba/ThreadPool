#ifndef TIMEDEXECUTORINTERFACE_H
#define TIMEDEXECUTORINTERFACE_H

#include <functional>

class TimedExecutorInterface
{
  public:
    TimedExecutorInterface() = default;
    virtual ~TimedExecutorInterface() = default;
    
    struct TimeoutException {}; 
    typedef std::function<void()> TCallable;

    virtual void ExecuteWithTimeout(TCallable&& aCallable, int aMilliseconds) = 0; 
};


#ifdef _WIN32
#include "Win32TimedExecutor.h"
typedef Win32TimedExecutor TimedExecutor;
#elif defined __linux__
#include "PosixTimedExecutor.h"
typedef PosixTimedExecutor<> TimedExecutor;
#endif

#endif
