#ifndef WORKITEM_H
#define WORKITEM_H

#include <string>

class WorkItem
{
  public:
    virtual void Call() const = 0;
    virtual ~WorkItem() = default;

    WorkItem(int aExecutionTimeout, std::string aId) 
      : iExecutionTimeout(aExecutionTimeout), iId(std::move(aId))						            
    {}

    int GetTimeoutInMilliseconds() const
    {
      return iExecutionTimeout;
    }

    std::string GetId() const
    {
      return iId;
    }

  private:
    int iExecutionTimeout;
    std::string iId;
};

template<typename taCallable>
class CallableWorkItem : public WorkItem
{
  public:
    CallableWorkItem(taCallable&& aCallable, int aExecutionTimeout, std::string aId = std::string(""))
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
