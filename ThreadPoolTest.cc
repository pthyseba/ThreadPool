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
  ThreadPool::ThreadPool t(1);//, t2(10);
  t.Push([](){throw "test exception";}, 1000, 0);
  t.Push([](){ return 2; }, 0, 1);
  t.Push([](){ int i =0; for(;;) i++; }, 2000, 2);
  t.Push([](){ int i =0; for(;;) i++; }, 2500, 3);
  t.Push([](){ volatile int i =0; while(i < 1000000) i++; }, 0, 4);
  t.Push(A(), 0000, 5);

  usleep(10000000);
  t.TryCancel(5);

  usleep(2000000);
  t.Stop();
  return 0;
}
