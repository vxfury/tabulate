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
#include <mutex>              // std::mutex, std::scoped_lock
#include <queue>              // std::queue
#include <thread>             // std::this_thread, std::thread
#include <type_traits>        // std::decay_t, std::enable_if_t, std::is_void_v, std::invoke_result_t
#include <utility>            // std::move, std::swap
#include <condition_variable> // std::condition_variable

#define USE_CONDITION_VARIABLE

namespace multiprocessing
{
/**
 * @brief A C++17 thread pool class. The user submits tasks to be executed into a queue. Whenever a thread becomes
 * available, it pops a task from the queue and executes it. Each task is automatically assigned a future, which can be
 * used to wait for the task to finish executing and/or obtain its eventual return value.
 */
class threadpool {
  public:
    using size_type = std::uint_fast32_t;

    threadpool(size_type concurrency = std::thread::hardware_concurrency())
        : worker_count(concurrency ? concurrency : std::thread::hardware_concurrency()),
          workers(new std::thread[worker_count])
    {
        for (size_type i = 0; i < worker_count; i++) {
            workers[i] = std::thread(&threadpool::worker, this);
        }
    }

    ~threadpool()
    {
#ifndef USE_CONDITION_VARIABLE
        // wait tasks
        while (true) {
            if (!paused) {
                if (tasks_total == 0) break;
            } else {
                if (get_tasks_running() == 0) break;
            }
            __idle();
        }
#endif

        // join workers
        shutdown = true;
#ifdef USE_CONDITION_VARIABLE
        queue_cond.notify_all();
#endif
        for (size_type i = 0; i < worker_count; i++) {
            workers[i].join();
        }
    }

    size_type get_tasks_running() const
    {
        const std::scoped_lock lock(queue_mutex);
        return tasks_total - tasks.size();
    }

    template <typename T, typename F>
    void parallelize(T first_index, T last_index, const F &loop, size_type num_tasks = 0)
    {
        if (num_tasks == 0) num_tasks = worker_count;
        if (last_index < first_index) std::swap(last_index, first_index);
        size_t total_size = last_index - first_index + 1;
        size_t block_size = total_size / num_tasks;
        if (block_size == 0) {
            block_size = 1;
            num_tasks = (size_type)total_size > 1 ? (size_type)total_size : 1;
        }
        std::atomic<size_type> blocks_running = 0;
        for (size_type t = 0; t < num_tasks; t++) {
            T start = (T)(t * block_size + first_index);
            T end = (t == num_tasks - 1) ? last_index : (T)((t + 1) * block_size + first_index - 1);
            blocks_running++;
            push([start, end, &loop, &blocks_running] {
                for (T i = start; i <= end; i++) loop(i);
                blocks_running--;
            });
        }
        while (blocks_running != 0) {
            __idle();
        }
    }

    template <typename F>
    void push(const F &task)
    {
        tasks_total++;
        {
            const std::scoped_lock lock(queue_mutex);
            tasks.push(std::function<void()>(task));
        }
#ifdef USE_CONDITION_VARIABLE
        queue_cond.notify_one();
#endif
    }

    template <typename F, typename... A>
    void push(const F &task, const A &...args)
    {
        push([task, args...] {
            task(args...);
        });
    }

    /**
     * @brief Submit a function with zero or more arguments and no return value into the task queue, and get an
     * std::future<bool> that will be set to true upon completion of the task.
     *
     * @tparam F The type of the function.
     * @tparam A The types of the zero or more arguments to pass to the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to check if the function has finished its execution.
     */
    template <typename F, typename... A,
              typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>>>
    std::future<bool> submit(const F &task, const A &...args)
    {
        std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
        std::future<bool> future = promise->get_future();
        push([task, args..., promise] {
            task(args...);
            promise->set_value(true);
        });
        return future;
    }

    /**
     * @brief Submit a function with zero or more arguments and a return value into the task queue, and get a future for
     * its eventual returned value.
     *
     * @tparam F The type of the function.
     * @tparam A The types of the zero or more arguments to pass to the function.
     * @tparam R The return type of the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to obtain the function's returned value, waiting for it to finish its execution
     * if needed.
     */
    template <typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>,
              typename = std::enable_if_t<!std::is_void_v<R>>>
    std::future<R> submit(const F &task, const A &...args)
    {
        std::shared_ptr<std::promise<R>> promise(new std::promise<R>);
        std::future<R> future = promise->get_future();
        push([task, args..., promise] {
            promise->set_value(task(args...));
        });
        return future;
    }

    /**
     * @brief An atomic variable indicating to the workers to pause. When set to true, the workers temporarily stop
     * popping new tasks out of the queue, although any tasks already executed will keep running until they are done.
     * Set to false again to resume popping tasks.
     */
    std::atomic<bool> paused = false;

#ifndef USE_CONDITION_VARIABLE
    /**
     * @brief The duration, in microseconds, that the worker function should sleep for when it cannot find any tasks in
     * the queue. If set to 0, then instead of sleeping, the worker function will execute std::this_thread::yield() if
     * there are no tasks in the queue. The default value is 1000.
     */
    size_type sleep_duration = 1000;
#endif

  private:
#ifndef USE_CONDITION_VARIABLE
    /**
     * @brief Try to pop a new task out of the queue.
     *
     * @param task A reference to the task. Will be populated with a function if the queue is not empty.
     * @return true if a task was found, false if the queue is empty.
     */
    bool pop(std::function<void()> &task)
    {
        const std::scoped_lock lock(queue_mutex);
        if (tasks.empty())
            return false;
        else {
            task = std::move(tasks.front());
            tasks.pop();
            return true;
        }
    }
#endif

    /**
     * @brief Sleep for sleep_duration microseconds. If that variable is set to zero, yield instead.
     *
     */
    inline void __idle()
    {
#ifndef USE_CONDITION_VARIABLE
        if (sleep_duration) {
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_duration));
        } else {
            std::this_thread::yield();
        }
#else
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        queue_cond.wait(lock, [this] {
            return shutdown || !tasks.empty();
        });
#endif
    }


    void worker()
    {
#ifdef USE_CONDITION_VARIABLE
        for (;;) {
#else
        while (!shutdown) {
#endif
            std::function<void()> task;

#ifndef USE_CONDITION_VARIABLE
            if (!paused && pop(task)) {
                task();
                tasks_total--;
            } else {
                __idle();
            }
#else
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                queue_cond.wait(lock, [this] {
                    return shutdown || !tasks.empty();
                });
                if (shutdown && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            task();
            tasks_total--;
#endif
        }
    }

    std::atomic<bool> shutdown = false;
    std::atomic<size_type> tasks_total = 0;

    mutable std::mutex queue_mutex;
    std::queue<std::function<void()>> tasks;
#ifdef USE_CONDITION_VARIABLE
    mutable std::condition_variable queue_cond;
#endif

    size_type worker_count;
    std::unique_ptr<std::thread[]> workers;
};
} // namespace multiprocessing
