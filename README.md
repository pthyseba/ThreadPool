# ThreadPool
ThreadPool implementation with timeout mechanism.
Timeout control flow is guided by setjmp/longjmp, so make sure NOT to allocate resources in the callable operator().
