#ifndef WORKITEM_H
#define WORKITEM_H

#include <string>

class WorkItem
{
  public:
    virtual void Call() const = 0;
    virtual ~WorkItem() = default;

    WorkItem(int aExecutionTimeout, int aId) 
      : iExecutionTimeout(aExecutionTimeout), iId(aId)						            
    {}

    int GetTimeoutInMilliseconds() const
    {
      return iExecutionTimeout;
    }

    int GetId() const
    {
      return iId;
    }

  private:
    int iExecutionTimeout;
    int iId;
};

template<typename taCallable>
class CallableWorkItem : public WorkItem
{
  public:
    CallableWorkItem(taCallable&& aCallable, int aExecutionTimeout, int aId = 0)
      : WorkItem(aExecutionTimeout, aId), iCallable(std::forward<taCallable>(aCallable))
    {}

    ~CallableWorkItem() {}

    void Call() const override
    {
      iCallable();
    }

  private:
    taCallable iCallable;
};

#endif
