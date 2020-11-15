#include "ThreadPool.h"

#ifdef _WIN32
#define usleep(a) Sleep(a/1000)
#else
#include "unistd.h"
#endif

class A
{
  public:
    void operator()() const
    {
      int i=0;
      for (;;) i++;	   
    }
};

int main(int argc, char** argv)
{
  ThreadPool::ThreadPool t(4);//, t2(10);
  t.Push([](){throw "test exception";}, 1000, "exception");
  t.Push([](){ return 2; }, 0, "SimpleReturn");
  t.Push([](){ int i =0; for(;;) i++; }, 2000, "LoopTest");
  t.Push([](){ int i =0; for(;;) i++; }, 2500, "LoopTest2");
  t.Push([](){ volatile int i =0; while(i < 1000000) i++; }, 0, "LoopTest3");
  t.Push(A(), 3000, "t.A()");

  usleep(5000000);
  t.Stop();
  return 0;
}
