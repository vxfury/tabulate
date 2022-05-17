/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>             // std::atomic
#include <chrono>             // std::chrono
#include <cstdint>            // std::int_fast64_t, std::uint_fast32_t
#include <functional>         // std::function
#include <future>             // std::future, std::promise
#include <memory>             // std::shared_ptr, std::unique_ptr
#include <mutex>              // std::mutex, std::lock_guard
#include <queue>              // std::queue
#include <thread>             // std::this_thread, std::thread
#include <type_traits>        // std::decay_t, std::enable_if_t, std::is_void_v, std::invoke_result_t
#include <utility>            // std::move, std::swap
#include <condition_variable> // std::condition_variable
#include <iostream>

#ifndef THREADPOOL_TRACE
#define THREADPOOL_TRACE(...)
#endif

namespace multiprocessing
{
/**
 * @brief A C++17 thread pool class. The user submits tasks to be executed into a queue. Whenever a thread becomes
 * available, it pops a task from the queue and executes it. Each task is automatically assigned a future, which can be
 * used to wait for the task to finish executing and/or obtain its eventual return value.
 */

/**
 * if your threads in the thread pool are constantly fed with tasks and you need fast response time, then yield is what
 * you want, but yield will burn cpu cycles no matter what the waiting thread is doing. if not, you can use the
 * conditional approach, threads will sleep until a task is ready (note though, a conditional can wake a thread, even if
 * no ready signal was sent), the response time might be slower, but you will not burn cpu cycles.
 *
 * I would recommend the conditional approach, and if the reaction time is too slow, switch to yield.
 *
 */
enum {
    CONDITION_VARIABLE,
    YIELD_OR_SCHED_DURATION,
};

template <int strategy = CONDITION_VARIABLE>
class threadpool {
  public:
    using size_type = unsigned int;
    threadpool(size_type concurrency = std::thread::hardware_concurrency())
        : paused(false),
          stopped(false),
          duration(10),
          concurrency(concurrency),
          workers(new std::thread[concurrency]),
          unfinished_task_size(0)
    {
        for (size_type i = 0; i < concurrency; i++) {
            workers[i] = std::thread(&threadpool::__worker, this);
        }
    }
    ~threadpool()
    {
        shutdown();
    }

    void pause()
    {
        paused = true;
    }

    void resume()
    {
        paused = false;
        if constexpr (strategy == CONDITION_VARIABLE) {
            queue_cond.notify_all();
        }
    }

    void wait()
    {
        while (true) {
            if (!paused) {
                if (unfinished_task_size == 0) {
                    THREADPOOL_TRACE("All tasks have been executed");
                    break;
                }
            } else {
                if (get_task_size_running() == 0) {
                    THREADPOOL_TRACE("No task running");
                    break;
                }
            }

            if constexpr (strategy == CONDITION_VARIABLE) {
                THREADPOOL_TRACE("Idle");
                std::unique_lock<std::mutex> lock(queue_lock);
                queue_cond.wait(lock, [this] {
                    return stopped || (!paused || !task_queue.empty());
                });
            }
            if constexpr (strategy == YIELD_OR_SCHED_DURATION) {
                THREADPOOL_TRACE("Idle");
                if (duration == 0) {
                    std::this_thread::yield();
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(duration));
                }
            }
        }
    }

    void shutdown()
    {
        if (!stopped) {
            stopped = true;
            if constexpr (strategy == CONDITION_VARIABLE) {
                queue_cond.notify_all();
            }

            for (size_type i = 0; i < concurrency; i++) {
                workers[i].join();
            }
        }
    }

    void set_duration(size_t duration_ms)
    {
        duration = duration_ms;
    }

    size_t get_duration()
    {
        return duration;
    }

    void reset(size_type worker_size = std::thread::hardware_concurrency())
    {
#if 0
        bool was_paused = paused;
        if (!was_paused) {
            pause();
        }
        wait();
        for (size_type i = 0; i < concurrency; i++) {
            workers[i].join();
        }
#endif
        shutdown();
        concurrency = worker_size;
        workers.reset(new std::thread[concurrency]);
        for (size_type i = 0; i < concurrency; i++) {
            workers[i] = std::thread(&threadpool::__worker, this);
        }
#if 0
        if (!was_paused) {
            resume();
        }
#endif
    }

    bool is_alive()
    {
        return !stopped;
    }

    bool is_active()
    {
        return !stopped && !paused;
    }

    size_type get_worker_size() const
    {
        return concurrency;
    }

    size_type get_task_size_unfinished() const
    {
        return unfinished_task_size;
    }

    size_type get_task_size_queued() const
    {
        std::lock_guard<std::mutex> guard(queue_lock);
        return task_queue.size();
    }

    size_type get_task_size_running() const
    {
        std::lock_guard<std::mutex> guard(queue_lock);
        return unfinished_task_size - task_queue.size();
    }

    template <typename T1, typename T2, typename TaskLoop>
    void parallelize(T1 first_index, T2 index_after_last, const TaskLoop &task_loop, size_type num_blocks = 0)
    {
        typedef std::common_type_t<T1, T2> T;
        T the_first_index = (T)first_index;
        T last_index = (T)index_after_last;
        if (the_first_index == last_index) return;
        if (last_index < the_first_index) {
            T temp = last_index;
            last_index = the_first_index;
            the_first_index = temp;
        }
        last_index--;
        if (num_blocks == 0) num_blocks = concurrency;
        size_t total_size = (size_t)(last_index - the_first_index + 1);
        size_t block_size = (size_t)(total_size / num_blocks);
        if (block_size == 0) {
            block_size = 1;
            num_blocks = (unsigned int)total_size > 1 ? (unsigned int)total_size : 1;
        }
        std::atomic<unsigned int> blocks_running = 0;
        for (unsigned int t = 0; t < num_blocks; t++) {
            T start = ((T)(t * block_size) + the_first_index);
            T end = (t == num_blocks - 1) ? last_index + 1 : ((T)((t + 1) * block_size) + the_first_index);
            blocks_running++;
            push([start, end, &task_loop, &blocks_running] {
                task_loop(start, end);
                blocks_running--;
            });
        }
        while (blocks_running != 0) {
            if constexpr (strategy == CONDITION_VARIABLE) {
                std::unique_lock<std::mutex> lock(queue_lock);
                queue_cond.wait(lock, [this] {
                    return stopped || (!paused || !task_queue.empty());
                });
            }
            if constexpr (strategy == YIELD_OR_SCHED_DURATION) {
                if (duration == 0) {
                    std::this_thread::yield();
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(duration));
                }
            }
        }
    }

    template <typename Task>
    void push(const Task &task)
    {
        unfinished_task_size++;
        {
            std::lock_guard<std::mutex> guard(queue_lock);
            task_queue.push(std::function<void()>(task));
        }
        if constexpr (strategy == CONDITION_VARIABLE) {
            queue_cond.notify_one();
        }
    }

    template <typename Task, typename... Args>
    void push(const Task &task, Args... args)
    {
        push([task, &args...] {
            task(args...);
        });
    }

    template <typename T, typename... Args, void (T::*Task)(Args...)>
    void push(typename T::Task *task, Args... args)
    {
        push([task, &args...] {
            task(args...);
        });
    }

    /**
     * @brief Submit a function with zero or more arguments and no return value into the task queue, and get an
     * std::future<bool> that will be set to true upon completion of the task.
     *
     * @tparam Task The type of the function.
     * @tparam Args The types of the zero or more arguments to pass to the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to check if the function has finished its execution.
     */
    template <
        typename Task, typename... Args,
        typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<std::decay_t<Task>, std::decay_t<Args>...>>>>
    std::future<bool> submit(const Task &task, const Args &...args)
    {
        std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
        std::future<bool> future = promise->get_future();
        push([task, args..., promise] {
            try {
                task(args...);
                promise->set_value(true);
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                }
            }
        });
        return future;
    }

    /**
     * @brief Submit a function with zero or more arguments and a return value into the task queue, and get a future for
     * its eventual returned value.
     *
     * @tparam Task The type of the function.
     * @tparam Args The types of the zero or more arguments to pass to the function.
     * @tparam Result The return type of the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to obtain the function's returned value, waiting for it to finish its execution
     * if needed.
     */
    template <typename Task, typename... Args,
              typename Result = std::invoke_result_t<std::decay_t<Task>, std::decay_t<Args>...>,
              typename = std::enable_if_t<!std::is_void_v<Result>>>
    std::future<Result> submit(const Task &task, const Args &...args)
    {
        std::shared_ptr<std::promise<Result>> promise(new std::promise<Result>);
        std::future<Result> future = promise->get_future();
        push([task, &args..., promise] {
            try {
                promise->set_value(task(args...));
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                }
            }
        });
        return future;
    }

  public:
    void __worker()
    {
        if constexpr (strategy == CONDITION_VARIABLE) {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_lock);
                    queue_cond.wait(lock, [this] {
                        return stopped || (!paused && !task_queue.empty());
                    });
                    if (stopped && task_queue.empty()) {
                        return;
                    }

                    task = std::move(task_queue.front());
                    task_queue.pop();
                }

                task();
                unfinished_task_size--;
            }
        } else {
            while (!stopped) {
                auto pop_task = [&](std::function<void()> &task) {
                    std::lock_guard<std::mutex> guard(queue_lock);
                    if (paused || task_queue.empty()) {
                        return false;
                    } else {
                        task = std::move(task_queue.front());
                        task_queue.pop();
                        return true;
                    }
                };

                std::function<void()> task;
                if (pop_task(task)) {
                    task();
                    unfinished_task_size--;
                } else {
                    if (duration == 0) {
                        std::this_thread::yield();
                    } else {
                        std::this_thread::sleep_for(std::chrono::microseconds(duration));
                    }
                }
            }
        }

#if 0
        while (true) {
            if constexpr (strategy == CONDITION_VARIABLE) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_lock);
                    queue_cond.wait(lock, [this] {
                        return stopped || (!paused && !task_queue.empty());
                    });
                    if (stopped && task_queue.empty()) {
                        return;
                    }

                    task = std::move(task_queue.front());
                    task_queue.pop();
                }

                task();
                unfinished_task_size--;
            } else {
                auto pop_task = [&](std::function<void()> &task) {
                    std::lock_guard<std::mutex> guard(queue_lock);
                    if (paused || task_queue.empty()) {
                        return false;
                    } else {
                        task = std::move(task_queue.front());
                        task_queue.pop();
                        return true;
                    }
                };

                std::function<void()> task;
                if (pop_task(task)) {
                    task();
                    unfinished_task_size--;
                } else if (!stopped) {
                    if (duration == 0) {
                        std::this_thread::yield();
                    } else {
                        std::this_thread::sleep_for(std::chrono::microseconds(duration));
                    }
                } else {
                    return;
                }
            }
        }
#endif
    }

    /**
     * An atomic variable indicating to the workers to pause. When set to true, the workers temporarily stop
     * popping new tasks out of the queue, although any tasks already executed will keep running until they are done.
     * Set to false again to resume popping tasks.
     */
    std::atomic<bool> paused;
    std::atomic<bool> stopped;

    size_t duration;

    size_type concurrency;
    std::unique_ptr<std::thread[]> workers;

    mutable std::mutex queue_lock;
    mutable std::condition_variable queue_cond;
    std::queue<std::function<void()>> task_queue;

    std::atomic<size_type> unfinished_task_size; /* number of tasks that not finished, queued or running */
};
} // namespace multiprocessing
