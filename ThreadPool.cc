#include "ThreadPool.h"
#include <unistd.h>
#include <iostream>

class A
{
	public:
		A() { std::cout << "Constructor" << std::endl; }
		A(A&& aOther)
	 	{ std::cout << "Move constructor" << std::endl;}

		~A() {std::cout << "Destructor" << std::endl;}
		void operator()() const
		{
                   std::cout << "A in thread " << GetThreadId () << std::endl;
		   usleep(3000000);
		}
};

int main(int argc, char** argv)
{
  ThreadPool::ThreadPool t(4), t2(10);
  t.Push([](){std::cout << "Returning from thread " << GetThreadId() << std::endl; return 2; });
  t.Push([](){std::cout << "Test in thread " << GetThreadId() << std::endl; usleep(3000000); std::cout << "Test finished" << std::endl;},2000);
  t.Push([](){std::cout << "Test2 in thread " << GetThreadId() << std::endl; usleep(3000000); std::cout << "Test2 finished" << std::endl;},2000);
  t.Push([](){std::cout << "Test3 in thread " << GetThreadId() << std::endl; usleep(1000000); std::cout << "Test3 finished" << std::endl;});
  t.Push(A(), 2000);
  t.Push([](){usleep(500000); throw "test exception";}, 1000);
  t2.Push(A(),2500);
  usleep(5000000);
  t.Stop();
  return 0;
}
