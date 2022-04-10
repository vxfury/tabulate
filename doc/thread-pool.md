# A C++17 Thread Pool for High-Performance Scientific Computing

Reference [GitHub repository](https://github.com/bshoshany/thread-pool), and [companion paper](https://arxiv.org/abs/2105.00613)

We present a modern C++17-compatible thread pool implementation, built from scratch with high-performance scientific computing in mind. The thread pool is implemented as a single lightweight and self-contained class, and does not have any dependencies other than the C++17 standard library, thus allowing a great degree of portability. In particular, our implementation does not utilize OpenMP or any other high-level multithreading APIs, and thus gives the programmer precise low-level control over the details of the parallelization, which permits more robust optimizations. The thread pool was extensively tested on both AMD and Intel CPUs with up to 40 cores and 80 threads. This paper provides motivation, detailed usage instructions, and performance tests. The code is freely available in the [GitHub repository](https://github.com/bshoshany/thread-pool). This `README.md` file contains roughly the same content as the [companion paper](https://arxiv.org/abs/2105.00613).

## Motivation

Multithreading is essential for modern high-performance computing. Since C++11, the C++ standard library has included built-in low-level multithreading support using constructs such as `std::thread`. However, `std::thread` creates a new thread each time it is called, which can have a significant performance overhead. Furthermore, it is possible to create more threads than the hardware can handle simultaneously, potentially resulting in a substantial slowdown.

This library contains a thread pool class, `threadpool`, which avoids these issues by creating a fixed pool of threads once and for all, and then reusing the same threads to perform different tasks throughout the lifetime of the pool. By default, the number of threads in the pool is equal to the maximum number of threads that the hardware can run in parallel.

The user submits tasks to be executed into a queue. Whenever a thread becomes available, it pops a task from the queue and executes it. Each task is automatically assigned an `std::future`, which can be used to wait for the task to finish executing and/or obtain its eventual return value.

In addition to `std::thread`, the C++ standard library also offers the higher-level construct `std::async`, which may internally utilize a thread pool - but this is not guaranteed, and in fact, currently only the MSVC implementation of `std::async` uses a thread pool, while GCC and Clang do not. Using our custom-made thread pool class instead of `std::async` allows the user more control, transparency, and portability.

High-level multithreading APIs, such as OpenMP, allow simple one-line automatic parallelization of C++ code, but they do not give the user precise low-level control over the details of the parallelization. The thread pool class presented here allows the programmer to perform and manage the parallelization at the lowest level, and thus permits more robust optimizations, which can be used to achieve considerably higher performance.

As demonstrated in the performance tests [below](#performance-tests), using our thread pool class we were able to saturate the upper bound of expected speedup for matrix multiplication and generation of random matrices. These performance tests were performed on 12-core / 24-thread and 40-core / 80-thread systems using GCC on Linux.

## Overview of features

* **Fast:**
  * Built from scratch with maximum performance in mind.
  * Suitable for use in high-performance computing nodes with a very large number of CPU cores.
  * Compact code, to reduce both compilation time and binary size.
  * Reusing threads avoids the overhead of creating and destroying them for individual tasks.
  * A task queue ensures that there are never more threads running in parallel than allowed by the hardware.
* **Lightweight:**
  * Only ~180 lines of code, excluding comments, blank lines, and the two optional helper classes.
  * Single header file: simply `#include "threadpool.hpp"`.
  * Header-only: no need to install or build the library.
  * Self-contained: no external requirements or dependencies. Does not require OpenMP or any other multithreading APIs. Only uses the C++ standard library, and works with any C++17-compliant compiler.
* **Easy to use:**
  * Very simple operation, using a handful of member functions.
  * Every task submitted to the queue automatically generates an `std::future`, which can be used to wait for the task to finish executing and/or obtain its eventual return value.
  * Optionally, tasks may also be submitted without generating a future, sacrificing convenience for greater performance.
  * The code is thoroughly documented using Doxygen comments - not only the interface, but also the implementation, in case the user would like to make modifications.
* **Additional features:**
  * Automatically parallelize a loop into any number of parallel tasks.
  * Easily wait for all tasks in the queue to complete.
  * Change the number of threads in the pool safely and on-the-fly as needed.
  * Fine-tune the sleep duration of each thread's worker function for optimal performance.
  * Monitor the number of queued and/or running tasks.
  * Pause and resume popping new tasks out of the queue.
  * Synchronize output to a stream from multiple threads in parallel using the `synced_stream` helper class.
  * Easily measure execution time for benchmarking purposes using the `timer` helper class.

## Compiling and compatibility

This library should successfully compile on any C++17 standard-compliant compiler, on all operating systems for which such a compiler is available. Compatibility was verified with a 12-core / 24-thread AMD Ryzen 9 3900X CPU at 3.8 GHz using the following compilers and platforms:

* GCC v10.2.0 on Windows 10 build 19042.928.
* GCC v10.3.0 on Ubuntu 21.04.
* Clang v11.0.0 on Windows 10 build 19042.928 and Ubuntu 21.04.
* MSVC v14.28.29910 on Windows 10 build 19042.928.

In addition, this library was tested on a [Compute Canada](https://www.computecanada.ca/) node equipped with two 20-core / 40-thread Intel Xeon Gold 6148 CPUs at 2.4 GHz, for a total of 40 cores and 80 threads, running CentOS Linux 7.6.1810, using the following compilers:

* GCC v9.2.0
* Intel C++ Compiler (ICC) v19.1.3.304

As this library requires C++17 features, the code must be compiled with C++17 support. For GCC, Clang, and ICC, use the `-std=c++17` flag. For MSVC, use `/std:c++17`. On Linux, you will also need to use the `-pthread` flag with GCC, Clang, or ICC to enable the POSIX threads library.

## Getting started

### Including the library

To use the thread pool library, simply include the header file:

```cpp
#include "threadpool.hpp"
```

The thread pool will now be accessible via the `threadpool` class.

### Constructors

The default constructor creates a thread pool with as many threads as the hardware can handle concurrently, as reported by the implementation via `std::thread::hardware_concurrency()`. With a hyperthreaded CPU, this will be twice the number of CPU cores. This is probably the constructor you want to use. For example:

```cpp
// Constructs a thread pool with as many threads as available in the hardware.
threadpool pool;
```

Optionally, a number of threads different from the hardware concurrency can be specified as an argument to the constructor. However, note that adding more threads than the hardware can handle will **not** improve performance, and in fact will most likely hinder it. This option exists in order to allow using **less** threads than the hardware concurrency, in cases where you wish to leave some threads available for other processes. For example:

```cpp
// Constructs a thread pool with only 12 threads.
threadpool pool(12);
```

If your program's main thread only submits tasks to the thread pool and waits for them to finish, and does not perform any computationally intensive tasks on its own, then it is recommended to use the default value for the number of threads. This ensures that all of the threads available in the hardware will be put to work while the main thread waits.

However, if your main thread does perform computationally intensive tasks on its own, then it is recommended to use the value `std::thread::hardware_concurrency() - 1` for the number of threads. In this case, the main thread plus the thread pool will together take up exactly all the threads available in the hardware.

## Submitting and waiting for tasks

### Submitting tasks to the queue with futures

A task can be any function, with zero or more arguments, and with or without a return value. Once a task has been submitted to the queue, it will be executed as soon as a thread becomes available. Tasks are executed in the order that they were submitted (first-in, first-out).

The member function `submit()` is used to submit tasks to the queue. The first argument is the function to execute, and the rest of the arguments are the arguments to pass to the function, if any. The return value is an `std::future` associated to the task. For example:

```cpp
// Submit a task without arguments to the queue, and get a future for it.
auto my_future = pool.submit(task);
// Submit a task with one argument to the queue, and get a future for it.
auto my_future = pool.submit(task, arg);
// Submit a task with two arguments to the queue, and get a future for it.
auto my_future = pool.submit(task, arg1, arg2);
```

Using `auto` for the return value of `submit()` is recommended, since it means the compiler will automatically detect which instance of the template `std::future` to use. The value of the future depends on whether the function has a return value or not:

* If the submitted function has a return value, then the future will be set to that value when the function finishes its execution.
* If the submitted function does not have a return value, then the future will be a `bool` that will be set to `true` when the function finishes its execution.

To wait until the future's value becomes available, use the member function `wait()`. To obtain the value itself, use the member function `get()`, which will also automatically wait for the future if it's not ready yet. For example:

```cpp
// Submit a task and get a future.
auto my_future = pool.submit(task);
// Do some other stuff while the task is executing.
do_stuff();
// Get the task's return value from the future, waiting for it to finish running if needed.
auto my_return_value = my_future.get();
```

### Submitting tasks to the queue without futures

Usually, it is best to submit a task to the queue using `submit()`. This allows you to wait for the task to finish and/or get its return value later. However, sometimes a future is not needed, for example when you just want to "set and forget" a certain task, or if the task already communicates with the main thread or with other tasks without using futures, such as via references or pointers. In such cases, you may wish to avoid the overhead involved in assigning a future to the task in order to increase performance.

The member function `push()` allows you to submit a task to the queue without generating a future for it. The task can have any number of arguments, but it cannot have a return value. For example:

```cpp
// Submit a task without arguments or return value to the queue.
pool.push(task);
// Submit a task with one argument and no return value to the queue.
pool.push(task, arg);
// Submit a task with two arguments and no return value to the queue.
pool.push(task, arg1, arg2);
```

### Manually waiting for all tasks to complete

To wait for a **single** submitted task to complete, use `submit()` and then use the `wait()` or `get()` member functions of the obtained future. However, in cases where you need to wait until **all** submitted tasks finish their execution, or if the tasks have been submitted without futures using `push()`, you can use the member function `wait_for_tasks()`.

Consider, for example, the following code:

```cpp
threadpool pool;
size_t a[100];
for (size_t i = 0; i < 100; i++)
    pool.push([&a, i] { a[i] = i * i; });
std::cout << a[50];
```

The output will most likely be garbage, since the task that modifies `a[50]` has not yet finished executing by the time we try to access that element (in fact, that task is probably still waiting in the queue). One solution would be to use `submit()` instead of `push()`, but perhaps we don't want the overhead of generating 100 different futures. Instead, simply adding the line

```cpp
pool.wait_for_tasks();
```

after the `for` loop will ensure - as efficiently as possible - that all tasks have finished running before we attempt to access any elements of the array `a`, and the code will print out the value `2500` as expected. (Note, however, that `wait_for_tasks()` will wait for **all** the tasks in the queue, including those that are unrelated to the `for` loop. Using `parallelize_loop()` would make much more sense in this particular case, as it will wait only for the tasks related to the loop.)

### Parallelizing loops

Consider the following loop:

```cpp
for (T i = start; i <= end; i++)
    loop(i);
```

where:

* `T` is any signed or unsigned integer type.
* `start` is the first index to loop over (inclusive).
* `end` is the last index to loop over (inclusive).
* `loop()` is a function that takes exactly one argument, the loop index, and has no return value.

This loop may be automatically parallelized and submitted to the thread pool's queue using the member function `parallelize_loop()` as follows:

```cpp
// Equivalent to the above loop, but will be automatically parallelized.
pool.parallelize_loop(start, end, loop);
```

The loop will be parallelized into a number of tasks equal to the number of threads in the pool, with each task executing the function `loop()` for a roughly equal number of indices. The main thread will then wait until all tasks generated by `parallelize_loop()` finish executing (and only those tasks - not any other tasks that also happen to be in the queue).

If desired, the number of parallel tasks may be manually specified using a fourth argument:

```cpp
// Parallelize the loop into 12 parallel tasks
pool.parallelize_loop(start, end, loop, 12);
```

For best performance, it is recommended to do your own benchmarks to find the optimal number of tasks for each loop (you can use the `timer` helper class - see [below](#measuring-execution-time)). Using less tasks than there are threads may be preferred if you are also running other tasks in parallel. Using more tasks than there are threads may improve performance in some cases.

As a simple example, the following code will calculate the squares of all integers from 0 to 99. Since there are 10 threads, the loop will be divided into 10 tasks, each calculating 10 squares:

```cpp
#include "threadpool.hpp"

int main()
{
    threadpool pool(10);
    size_t squares[100];
    pool.parallelize_loop(0, 99, [&squares](size_t i) { squares[i] = i * i; });
    std::cout << "16^2 = " << squares[16] << '\n';
    std::cout << "32^2 = " << squares[32] << '\n';
}
```

The output should be:

```none
16^2 = 256
32^2 = 1024
```

### Setting the worker function's sleep duration

The **worker function** is the function that controls the execution of tasks by each thread. It loops continuously, and with each iteration of the loop, checks if there are any tasks in the queue. If it finds a task, it pops it out of the queue and executes it. If it does not find a task, it will wait for a bit, by calling `std::this_thread::sleep_for()`, and then check the queue again. The public member variable `sleep_duration` controls the duration, in microseconds, that the worker function sleeps for when it cannot find a task in the queue.

The default value of `sleep_duration` is `1000` microseconds, or `1` millisecond. In our benchmarks, lower values resulted in high CPU usage when the workers were idle. The value of `1000` microseconds was roughly the minimum value needed to reduce the idle CPU usage to a negligible amount.

In addition, in our benchmarks this value resulted in moderately improved performance compared to lower values, since the workers check the queue - which is a costly process - less frequently. On the other hand, increasing the value even more could potentially cause the workers to spend too much time sleeping and not pick up tasks from the queue quickly enough, so `1000` is the "sweet spot".

However, please note that this value is likely unique to the particular system our benchmarks were performed on, and your own optimal value would depend on factors such as your OS and C++ implementation, the type, complexity, and average duration of the tasks submitted to the pool, and whether there are any other programs running at the same time. Therefore, it is strongly recommended to do your own benchmarks and find the value that works best for you.

If `sleep_duration` is set to `0`, then the worker function will execute `std::this_thread::yield()` instead of sleeping if there are no tasks in the queue. This will suggest to the OS that it should put this thread on hold and allow other threads to run instead. However, this also causes the worker functions to have high CPU usage when idle. On the other hand, for some applications this setting may provide better performance than sleeping - again, do your own benchmarks and find what works best for you.

### Pausing the workers

Sometimes you may wish to temporarily pause the execution of tasks, or perhaps you want to submit tasks to the queue but only start executing them at a later time. You can do this using the public member variable `paused`.

When `paused` is set to `true`, the workers will temporarily stop popping new tasks out of the queue. However, any tasks already executed will keep running until they are done, since the thread pool has no control over the internal code of your tasks. If you need to pause a task in the middle of its execution, you must do that manually by programming your own pause mechanism into the task itself. To resume popping tasks, set `paused` back to its default value of `false`.

Here is an example:

```cpp
#include "threadpool.hpp"

void sleep_half_second(const size_t &i, synced_stream *sync_out)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sync_out->println("Task ", i, " done.");
}

int main()
{
    synced_stream sync_out;
    threadpool pool(4);
    for (size_t i = 0; i < 8; i++)
        pool.push(sleep_half_second, i, &sync_out);
    sync_out.println("Submitted 8 tasks.");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    pool.paused = true;
    sync_out.println("Pool paused.");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sync_out.println("Still paused...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    for (size_t i = 8; i < 12; i++)
        pool.push(sleep_half_second, i, &sync_out);
    sync_out.println("Submitted 4 more tasks.");
    sync_out.println("Still paused...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    pool.paused = false;
    sync_out.println("Pool resumed.");
}
```

Assuming you have at least 4 hardware threads, the output will be similar to:

```none
Submitted 8 tasks.
Pool paused.
Task 0 done.
Task 1 done.
Task 2 done.
Task 3 done.
Still paused...
Submitted 4 more tasks.
Still paused...
Pool resumed.
Task 4 done.
Task 5 done.
Task 6 done.
Task 7 done.
Task 8 done.
Task 9 done.
Task 10 done.
Task 11 done.
```

Here is what happened. We initially submitted a total of 8 tasks to the queue. Since we waited for 250ms before pausing, the first 4 tasks have already started running, so they kept running until they finished. While the pool was paused, we submitted 4 more tasks to the queue, but they just waited at the end of the queue. When we resumed, the remaining 4 initial tasks were executed, followed by the 4 new tasks.

While the workers are paused, `wait_for_tasks()` will wait for the running tasks instead of all tasks (otherwise it would wait forever). This is demonstrated by the following program:

```cpp
#include "threadpool.hpp"

void sleep_half_second(const size_t &i, synced_stream *sync_out)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sync_out->println("Task ", i, " done.");
}

int main()
{
    synced_stream sync_out;
    threadpool pool(4);
    for (size_t i = 0; i < 8; i++)
        pool.push(sleep_half_second, i, &sync_out);
    sync_out.println("Submitted 8 tasks. Waiting for them to complete.");
    pool.wait_for_tasks();
    for (size_t i = 8; i < 20; i++)
        pool.push(sleep_half_second, i, &sync_out);
    sync_out.println("Submitted 12 more tasks.");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    pool.paused = true;
    sync_out.println("Pool paused. Waiting for the ", pool.get_tasks_running(), " running tasks to complete.");
    pool.wait_for_tasks();
    sync_out.println("All running tasks completed. ", pool.get_tasks_queued(), " tasks still queued.");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sync_out.println("Still paused...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sync_out.println("Still paused...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    pool.paused = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    sync_out.println("Pool resumed. Waiting for the remaining ", pool.get_tasks_total(), " tasks (", pool.get_tasks_running(), " running and ", pool.get_tasks_queued(), " queued) to complete.");
    pool.wait_for_tasks();
    sync_out.println("All tasks completed.");
}
```

The output should be similar to:

```none
Submitted 8 tasks. Waiting for them to complete.
Task 0 done.
Task 1 done.
Task 2 done.
Task 3 done.
Task 4 done.
Task 5 done.
Task 6 done.
Task 7 done.
Submitted 12 more tasks.
Pool paused. Waiting for the 4 running tasks to complete.
Task 8 done.
Task 9 done.
Task 10 done.
Task 11 done.
All running tasks completed. 8 tasks still queued.
Still paused...
Still paused...
Pool resumed. Waiting for the remaining 8 tasks (4 running and 4 queued) to complete.
Task 12 done.
Task 13 done.
Task 14 done.
Task 15 done.
Task 16 done.
Task 17 done.
Task 18 done.
Task 19 done.
```

The first `wait_for_tasks()`, which was called with `paused == false`, waited for all 8 tasks, both running and queued. The second `wait_for_tasks()`, which was called with `paused == true`, only waited for the 4 running tasks, while the other 8 tasks remained queued, and were not executed since the pool was paused. Finally, the third `wait_for_tasks()`, which was called with `paused == false`, waited for the remaining 8 tasks, both running and queued.

**Warning**: If the thread pool is destroyed while paused, any tasks still in the queue will never be executed.
