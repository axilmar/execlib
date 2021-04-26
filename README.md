# execlib

C++17 concurrent execution library.

It provides the following features:

* a background thread pool that can execute jobs (with job stealing for increased throughput).
* a simple `execute()` function that allows any task to be executed by a background worker thread.
* an `event` class which can be used for thread coordination.
* a `future<T>` class which can be used to deliver a result from one thread to another (if using the standard future+promise classes are not preferred).

## API
### Initialization/cleanup

Initializing and cleaning up the API can be done like this:

```cpp
#include "execlib.hpp"

int main() {
	//initialize
	execlib::initialize();
	
	//your code goes here
	
	//cleanup
	execlib::cleanup();
	return 0;
}
```

The number of threads can be specified as a parameter to the `initialize()` function. By default, the `initialize()` function creates as many threads as they are reported by `std::thread::hardware_concurrency()`.

### Execution

Executing a task requires writing an anonymous function and then passing it to the `execute()` function. Example:

```cpp
void hello_world() {
	execlib::execute([](){ std::cout << "hello world!\n"; });
}
```

### Notifications between threads

Threads can be notified that other threads have some results available via the following ways:

1. using `std::future<T>`, `std::promise<T>`. This is the standard way of communicating results.
2. using `execlib::event` or `execlib::future<T>`. These classes are very simple, simpler than what the standard gives, but also less capable (for example, they cannot curry exceptions).

#### The `event` class

Here is an example of how to use the `event` class:

```cpp
void some_long_task() {
    execlib::event e;
    execlib::execute([](){
    	some_long_computation();
    	e.set_and_notify_one();
    });
    e.wait();
}
```

#### The `future<T>` class

Here is an example of how to use the `future<T>` class:

```cpp
void some_long_task() {
    execlib::future<int> f;
    execlib::execute([](){
    	some_long_computation();
    	f.set_and_notify_one(1);
    });
    int r = f.wait();
}
```

### The best way to take advantage of this API...

In order to fully exploit this API, computations shall be broken down to individual steps that can be executed in parallel.

Computations that yield a result and then perform other computations shall be executed in nested `execute()` calls. In this way, the internal work threads can be kept busy and actually increase the aplication's performance.

For example, let's say we have a function named 'function1' which returns an integer, which must be fed to another function named 'function2'. The best way to achieve this is the following:

```cpp
void do_actions() {
	execlib::execute([](){
		int result = function1();
		execlib::execute([result](){ 
			function2(result); 
		});
	});
}
```
In this way, a natural graph of operations is created dynamically, and each part of the graph is naturally processed by multiple threads.

**NOTE: Using the `event` or `future<T>` classes shall be done with caution, because they might block the worker threads and decrease performance.**

## The algorithms

### The dispatch algorithm

Tasks are dispatched in round-robin fashion to all the background worker threads. Each thread has its own task queue.

### The work-stealing algorithm

Internally, each worker thread waits for jobs to appear in its own queue.

However, the queue may become empty and the thread might no longer have something to do.

Instead of waiting, the thread with an empty queue goes looking at the queues of other threads, starting from its neighbour threads. If it finds some jobs, then it takes those from these other queues and puts them in its own queue.

A thread takes half of the jobs of the first neighbour thread found to contain jobs.

In this way, threads are kept busy and they are only waiting if no other thread has anything to do.

### Lock contention

Each thread has a mutex that protects its queue.

Putting a task to this queue requires this mutex to be locked.

Getting a task from this queue requires this mutex to be locked.

Stealing items from a thread's queue requires the two mutexes of the source and destination threads to be locked.

In this way, lock contention is kept minimal.

## Frequently Asked Questions

### Can the background thread for a task be chosen?

No. 

The thread that will execute a task is random and depends on the load distribution. It would be meanigless to provide a task execution API and also provide the functionality of posting a task to the same thread, because that would defeat the purpose of parallelizing the task execution.

This deliberate design decision makes programming APIs harder, because all APIs shall be well-behaved in a threaded context.

It's an unvoidable tradeoff in the age of multiple CPU cores, in my humble opinion, and worth having, for computation-heavy applications. 

### Can the memory management for a task be customized?

Yes.  
  
The base class for all tasks is the class `task`. Implementing a sub-class of this class allows the customization of all the aspects of the class, including the way it is allocated/deleted.

The default memory management is global operator new/delete for tasks.

In most platforms, the default memory management might be more than enough because usually it is heavily optimized.
