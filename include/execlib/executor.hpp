#ifndef EXECLIB_JOB_EXECUTOR_HPP
#define EXECLIB_JOB_EXECUTOR_HPP


#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include "executor_internals.hpp"


namespace execlib {


    /**
     * Contains the mechanism for executing jobs in different threads.
     * It uses job stealing in order to avoid thread starving.
     */
    class executor : private executor_internals {
    public:
        /**
         * The constructor.
         * @param thread_count number of threads to use.
         * @exception std::invalid_argument thrown if thread_count is 0.
         */
        executor(size_t thread_count = std::thread::hardware_concurrency());

        /**
         * Signals all threads to stop execution, then waits for them to stop.
         * Remaining jobs are not executed.
         */
        ~executor();

        /**
         * Returns number of threads.
         * @return number of threads.
         */
        size_t thread_count() const { return m_queues.size(); }

        /**
         * Executes the given job.
         * The target thread is chosen in a round-robin fashion.
         * Memory for the job is allocated in the context of the target thread.
         */
        template <class F> void execute(F&& func) {
            using job_type = job_impl<F>;

            //next queue index, in round robin fashion
            const size_t queue_index = m_next_queue_index.fetch_add(1, std::memory_order_release) % m_queues.size();

            //the queue to put the job to
            queue* q = m_queues[queue_index];

            //allocate/init job/put job in queue, synchronized on queue
            {
                std::lock_guard lock_queue(get_mutex(q));

                //allocate memory for job; each queue has its own memory pool
                void* mem = alloc_memory_for_job(q, sizeof(job_type));

                //init job
                job* j = new (mem) job_type(get_queue_base(q), std::forward<F>(func));

                //put the job in the queue
                put_job(q, j);
            }

            //notify the queue listener
            notify_listener(q);
        }

        /**
         * Removes the current worker thread from the executor's active threads
         * and puts it in a deactivated thread list.
         * 
         * The current worker thread is replaced with another worker thread.
         *
         * It must be invoked at the beginning of a long-running job.
         *
         * This is useful for long-running jobs that may occupy a worker thread
         * for a long time: it allows the executor to continue executing jobs
         * on all queues, despite this thread blocking the worker thread.
         * 
         * @exception std::logic_error thrown if this is called outside of a worker thread.
         */
        static void release_current_worker_thread();

        /**
         * Returns the current executor.
         * @return the current executor, or null if this code is not executed by an executor.
         */
        static executor* get_current_executor();

        executor(const executor&) = delete;
        executor(executor&&) = delete;
        executor& operator = (const executor&) = delete;
        executor& operator = (executor&&) = delete;

    private:
        //queue defined in implementation file
        class queue;

        //worker thread defined in implementation file
        class worker_thread;

        //queues are used in a round-robin fashion
        std::atomic<size_t> m_next_queue_index{ 0 };

        //queues
        std::vector<queue*> m_queues;

        //worker thread mutex
        std::mutex m_worker_thread_mutex;

        //all worker threads
        std::vector<worker_thread*> m_worker_threads;

        //released worker threads
        std::vector<worker_thread*> m_released_worker_threads;

        //get mutex of queue
        static std::mutex& get_mutex(queue* q);

        //get queue base
        static queue_base* get_queue_base(queue* q);

        //allocate memory from queue
        static void* alloc_memory_for_job(queue* q, size_t size);

        //put job in queue
        static void put_job(queue* q, job* j);

        //notifies the queue listener that a new job is available
        static void notify_listener(queue* q);

        friend class _executor;
        friend class worker_thread;
    };


} //namespace execlib


#endif //EXECLIB_JOB_EXECUTOR_HPP
