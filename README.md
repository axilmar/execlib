# execlib

C++17 concurrent execution library.

## Example

```cpp
#include <iostream>
#include "execlib.hpp"

execlib::executor executor;

int factorial(int n) {
    return n > 0 ? n * factorial(n - 1) : 1;
}

void task(int i) {
    executor.execute([i]() {
        int r = factorial(i);
        std::cout << "factorial of " << i << " = " << r << std::endl;
    });
}

int main() {
    for(int i = 0; i < 100; ++i) {
        task(i);
    }
}
```

## Classes

### executor

Allows the execution of tasks in a worker thread; the number of worker threads are defined in the constructor.

### counter

Allows blocking on a variable until that variable reaches a specific value; useful for counting tasks.

## Algorithms

### The Scheduler

The thread scheduler is round robin: each new job is added to the next thread's queue, rolling back to the first thread if the last thread is reached.

### Job Stealing

In order to avoid thread starving, threads steal jobs from their neighbors, if they don't have any jobs to execute.

### Memory Allocation

Each thread has its own memory pool to allocate memory for jobs.

### Lock Contention

Each thread has its own mutex to protect its resources. There is no lock contention between threads, except when deallocating memory of stolen jobs.

