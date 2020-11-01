#ifndef WORKITEM_H
#define WORKITEM_H

class WorkItem
{
  public:
    virtual void Call() const = 0;
    virtual ~WorkItem() = default;

    WorkItem(int aExecutionTimeout) : iExecutionTimeout(aExecutionTimeout)						            
    {}

    int GetTimeoutInMilliseconds() const
    {
      return iExecutionTimeout;
    }

  private:
    int iExecutionTimeout;
};

template<typename taCallable>
class CallableWorkItem : public WorkItem
{
  public:
    CallableWorkItem(taCallable&& aCallable, int aExecutionTimeout)
      : WorkItem(aExecutionTimeout), iCallable(std::move(aCallable))
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
