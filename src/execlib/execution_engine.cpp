#include <thread>
#include <atomic>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <condition_variable>
#include <memory>
#include "execlib/execution_engine.hpp"


namespace execlib {


    //thread
    struct thread {
        //mutex
        std::mutex mutex;

        //wait condition
        std::condition_variable wait_cond;

        //queue
        std::deque<task*> queue;

        //stop flag
        bool stop{ false };

        //thread
        std::thread thread_object;
    };


    //current thread; tasks are dispatched in round robin fashion
    static std::atomic<size_t> curr_thread{ 0 };


    //threads
    static size_t thread_count{ 0 };
    static thread* threads;


    //constants
    static constexpr size_t MIN_QUEUE_SIZE_TO_STEAL = 4;


    //get task from thread
    static task* get_task(thread& thr) {
        std::lock_guard lock(thr.mutex);
        
        //if the queue is empty, don't do anything
        if (thr.queue.empty()) {
            return nullptr;
        }

        //take the task from the back
        task* tsk = thr.queue.back();
        thr.queue.pop_back();
        return tsk;
    }


    //steal task
    static task* steal_task(thread& thr) {
        const size_t thread_index = &thr - threads;

        //steal tasks from other threads
        for (size_t i = thread_index; i < thread_index + thread_count; ++i) {
            thread* other_thread = &threads[i % thread_count];

            {
                std::lock_guard other_lock(other_thread->mutex);

                //the other thread must have a non-trivial number of tasks
                if (other_thread->queue.size() < MIN_QUEUE_SIZE_TO_STEAL) {
                    continue;
                }

                //steal the half
                const size_t steal_size = other_thread->queue.size() / 2;

                {
                    std::lock_guard lock(thr.mutex);

                    //get the newest half of jobs from other thread 
                    thr.queue.insert(thr.queue.end(), other_thread->queue.end() - steal_size, other_thread->queue.end());

                    //remove newest half of jobs from other thread
                    other_thread->queue.erase(other_thread->queue.end() - steal_size, other_thread->queue.end());

                    //get task from back
                    task* tsk = thr.queue.back();
                    thr.queue.pop_back();
                    return tsk;
                }
            }
        }

        //no other threads contained any significant amount of jobs to steal
        return nullptr;
    }


    //wait for task
    static task* wait_for_task(thread& thr) {
        std::unique_lock lock(thr.mutex);

        //if stop, return null to exit the thread
        if (thr.stop) {
            return nullptr;
        }

        //if it's empty, wait
        while (thr.queue.empty()) {
            thr.wait_cond.wait(lock);

            //if stopped while waiting, return null to exit the thread
            if (thr.stop) {
                return nullptr;
            }
        }

        //found element
        task* tsk = thr.queue.back();
        thr.queue.pop_back();
        return tsk;
    }


    //thread procedure
    static void thread_proc(thread& thr) {
        task* tsk = nullptr;

        //loop until stopped
        for (;;) {
            //get task; if found, execute it and get more
            tsk = get_task(thr);
            if (tsk) {
                tsk->execute();
                delete tsk;
                continue;
            }

            //steal some tasks from other threads; if at least one is found, execute it and get more
            tsk = steal_task(thr);
            if (tsk) {
                tsk->execute();
                delete tsk;
                continue;
            }

            //no tasks could be stolen; wait for task; if found, execute it and get more
            tsk = wait_for_task(thr);
            if (tsk) {
                tsk->execute();
                delete tsk;
                continue;
            }

            //no task from waiting; it means the thread had been stopped
            return;
        }
    }


    //put task in thread queue
    static void put(thread& thr, task* tsk) { 
        //synchronized put
        {
            std::lock_guard lock(thr.mutex);
            thr.queue.push_front(tsk);
        }

        //notify the thread
        thr.wait_cond.notify_one();
    }


    //initialize with specific number of threads
    void initialize(const size_t tc) {
        //must have thread_count > 0
        if (tc == 0) {
            throw std::invalid_argument("thread_count must not be 0");
        }

        //create the thread objects
        thread_count = tc;
        threads = new thread[thread_count];

        //start the threads
        for (size_t i = 0; i < thread_count; ++i) {
            threads[i].thread_object = std::thread([i]() { thread_proc(threads[i]); });
        }
    }


    //initialize with hardware number of threads
    void initialize() {
        initialize(std::thread::hardware_concurrency());
    }


    //Stops the executing threads and cleans up the eecution engine.
    void cleanup() {
        //put the stop flag and notify the threads
        for (size_t i = 0; i < thread_count; ++i) {
            {
                std::lock_guard lock(threads[i].mutex);
                threads[i].stop = true;
            }
            threads[i].wait_cond.notify_one();
        }

        //wait for the threads to terminate
        for (size_t i = 0; i < thread_count; ++i) {
            threads[i].thread_object.join();
        }

        //delete any pending tasks
        for (size_t i = 0; i < thread_count; ++i) {
            for (task* tsk : threads[i].queue) {
                delete tsk;
            }
        }

        //delete the threads
        delete[] threads;
    }


    //Returns the number of threads used to execute tasks.
    size_t get_thread_count() {
        return thread_count;
    }


    //executes the task
    void execute(task* const t) noexcept {
        //get thread index in round robin fashion
        const size_t thread_index = curr_thread.fetch_add(1, std::memory_order_relaxed) % thread_count;

        //put the task in the thread's queue
        put(threads[thread_index], t);
    }


} //namespace execlib
