#include <stdexcept>
#include <memory_resource>
#include <deque>
#include <condition_variable>
#include "execlib/executor.hpp"


namespace execlib {


    class _executor {
    public:
        using worker_thread = executor::worker_thread;
    };


    //current executor
    static thread_local executor* current_executor = nullptr;


    //current worker thread
    static thread_local _executor::worker_thread* current_worker_thread = nullptr;


    //queue definition
    class executor::queue {
    public:
        //constructor.
        queue(size_t index) 
            : m_index(index)
            , m_jobs(std::pmr::polymorphic_allocator<job*>(&m_memory_pool))
        {
        }

        //returns the queue's mutex
        std::mutex& get_mutex() { return m_mutex; }

        //allocate memory
        void* alloc_memory(size_t size) {
            return m_memory_pool.allocate(size);
        }

        //free memory
        void free_memory(void* p, size_t size) {
            m_memory_pool.deallocate(p, size);
        }

        //check if empty
        bool empty() const {
            return m_jobs.empty();
        }

        //get job from queue
        job* get_job() {
            job* result = m_jobs.front();
            m_jobs.pop_front();
            return result;
        }

        //put job in queue
        void put_job(job* j) {
            m_jobs.push_back(j);
        }

        //notify the listener thread
        void notify_listener() {
            m_cond.notify_one();
        }

        //delete job
        void delete_job(job* j) {
            const size_t size = j->get_size();
            j->~job();
            free_memory(j, size);
        }

    private:
        //queue index
        const size_t m_index;

        //mutex that protects the queue
        std::mutex m_mutex;

        //memory pool; local to queue to avoid possible global lock contention from c++ memory allocation
        std::pmr::unsynchronized_pool_resource m_memory_pool;

        //jobs
        std::deque<job*, std::pmr::polymorphic_allocator<job*>> m_jobs;

        //for thread notifications
        std::condition_variable m_cond;

        friend class executor;
        friend class worker_thread;
    };


    //worker thread
    class executor::worker_thread {
    public:
        //starts the worker thread
        worker_thread(executor* ex, queue* q)
            : m_executor(ex)
            , m_queue(q)
            , m_thread(&worker_thread::run, this)
        {
        }

        //stops the worker thread and waits for its termination
        ~worker_thread() {
            stop();
            m_thread.join();
        }

    private:
        //executor for this thread
        executor* m_executor;

        //the current queue
        std::atomic<queue*> m_queue;

        //stop flag
        std::atomic<bool> m_stop{ false };

        //condition variable to wait when suspended
        std::condition_variable m_suspend_cond;

        //the thread
        std::thread m_thread;

        //stops this thread
        void stop() {
            m_stop.store(true, std::memory_order_release);
            queue* q = m_queue.load(std::memory_order_acquire);
            if (q) {
                q->m_cond.notify_one();
            }
            m_suspend_cond.notify_one();
        }

        //steal jobs from queue
        bool steal_jobs(queue* dst, queue* src) {
            //lock the source queue
            std::lock_guard lock(src->m_mutex);

            //the source queue must have at least 2 jobs
            if (src->m_jobs.size() < 2) {
                return false;
            }

            //define the range of jobs to steal
            auto begin = src->m_jobs.begin();
            auto end = begin + src->m_jobs.size() / 2;

            //insert jobs in destination
            dst->m_jobs.insert(dst->m_jobs.end(), begin, end);

            //remove jobs from source
            src->m_jobs.erase(begin, end);

            return true;
        }

        //steal jobs from executor
        void steal_jobs(queue* q) {
            for (size_t i = q->m_index + 1; i < m_executor->m_queues.size(); ++i) {
                if (steal_jobs(q, m_executor->m_queues[i])) {
                    return;
                }
            }
            for (size_t i = 0; i < q->m_index; ++i) {
                if (steal_jobs(q, m_executor->m_queues[i])) {
                    return;
                }
            }
        }

        //runs the thread
        void run() {
            std::mutex suspend_mutex;

            current_executor = m_executor;
            current_worker_thread = this;

            //runs until stop is requested
            for (;;) {
                queue* q = m_queue.load(std::memory_order_acquire);

                //if it has queue, then wait on jobs from queue
                if (q) {
                    job* j;
                    {
                        std::unique_lock lock_queue(q->m_mutex);

                        //if no jobs, steal jobs from other queues
                        if (q->m_jobs.empty()) {
                            steal_jobs(q);
                        }

                        //wait while the queue is empty
                        while (q->m_jobs.empty()) {
                            //if stop was requested, return
                            if (m_stop.load(std::memory_order_acquire)) {
                                return;
                            }

                            //if suspended mode is entered, wait
                            if (!m_queue.load(std::memory_order_acquire)) {
                                goto SUSPEND;
                            }

                            //wait for jobs or stop/suspend event
                            q->m_cond.wait(lock_queue);
                        }

                        //get job
                        j = q->get_job();
                    }

                    //execute and delete job
                    try {
                        j->invoke();
                    }
                    catch (...) {
                        q->delete_job(j);
                        throw;
                    }
                    q->delete_job(j);

                    //continue loop
                    continue;
                }

                //suspend thread
                SUSPEND:

                //wait until a queue is set
                do {
                    //if stop was requested, return
                    if (m_stop.load(std::memory_order_acquire)) {
                        return;
                    }

                    //wait for event
                    std::unique_lock suspend_lock(suspend_mutex);
                    m_suspend_cond.wait(suspend_lock);

                } while (!m_queue.load(std::memory_order_acquire));
            }
        }

        friend class executor;
    };


    //The constructor.
    executor::executor(size_t thread_count) {
        //check thread count
        if (thread_count == 0) {
            throw std::invalid_argument("thread count is 0");
        }

        //create queues/threads
        for (size_t i = 0; i < thread_count; ++i) {
            queue* q = new queue(i);
            m_queues.push_back(q);

            worker_thread* wt = new worker_thread(this, q);
            m_worker_threads.push_back(wt);
        }
    }

    //Signals all threads to stop execution, then waits for them to stop.
    executor::~executor() {
        //stop and delete worker threads
        {
            std::lock_guard lock(m_worker_thread_mutex);
            for (worker_thread* wt : m_worker_threads) {
                delete wt;
            }
        }

        //delete queues
        for (queue* q : m_queues) {
            delete q;
        }
    }


    //replace current worker thread with another thread
    void executor::release_current_worker_thread() {
        //check conditions
        if (!current_worker_thread) {
            throw std::logic_error("the current thread is not an executor worker thread");
        }

        //get the current queue
        queue* current_worker_thread_queue = current_worker_thread->m_queue.load(std::memory_order_acquire);

        //make the current thread suspended by nullifying its queue pointer
        current_worker_thread->m_queue.store(nullptr, std::memory_order_release);

        worker_thread* replacement_worker_thread;

        //lock the worker threads
        std::lock_guard lock(current_executor->m_worker_thread_mutex);

        //if there is a released worker thread, use that
        if (!current_executor->m_released_worker_threads.empty()) {
            replacement_worker_thread = current_executor->m_released_worker_threads.back();
            current_executor->m_released_worker_threads.pop_back();
            replacement_worker_thread->m_queue.store(current_worker_thread_queue, std::memory_order_release);
            replacement_worker_thread->m_suspend_cond.notify_one();
        }

        //else create new worker thread
        else {
            replacement_worker_thread = new worker_thread(current_executor, current_worker_thread_queue);
            current_executor->m_worker_threads.push_back(replacement_worker_thread);
        }

        //add the current thread to the released ones
        current_executor->m_released_worker_threads.push_back(current_worker_thread);
    }


    //get current thread executor
    executor* executor::get_current_executor() {
        return current_executor;
    }


    //get mutex of queue
    std::mutex& executor::get_mutex(queue* q) {
        return q->get_mutex();
    }


    //allocate memory from queue
    void* executor::alloc_memory(queue* q, size_t size) {
        return q->alloc_memory(size);
    }


    //put job in queue
    void executor::put_job(queue* q, job* j) {
        q->put_job(j);
    }


    //notifies the queue listener that a new job is available
    void executor::notify_listener(queue* q) {
        q->notify_listener();
    }


} //namespace execlib
