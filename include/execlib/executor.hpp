#ifndef EXECLIB_JOB_EXECUTOR_HPP
#define EXECLIB_JOB_EXECUTOR_HPP


#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <memory_resource>
#include <atomic>
#include <vector>


namespace execlib {


    /**
     * Contains the mechanism for executing jobs in different threads.
     * It uses job stealing in order to avoid thread starving.
     */
    class executor {
    public:
        /**
         * The constructor.
         * @param thread_count number of threads to use.
         * @exception std::invalid_argument thrown if thread_count is 0.
         */
        executor(size_t thread_count = std::thread::hardware_concurrency());

        executor(const executor&) = delete;
        executor(executor&&) = delete;

        /**
         * Signals all threads to stop execution, then waits for them to stop.
         * Remaining jobs are not executed.
         */
        ~executor();

        executor& operator = (const executor&) = delete;
        executor& operator = (executor&&) = delete;

        /**
         * Returns number of threads.
         * @return number of threads.
         */
        size_t thread_count() const { return m_threads.size(); }

        /**
         * Executes the given job.
         * The target thread is chosen in a round-robin fashion.
         * Memory for the job is allocated in the context of the target thread.
         */
        template <class J> void execute(J&& job) {
            using job_type = job_impl<J>;

            //get next index
            const size_t next_index = m_next_index.fetch_add(1, std::memory_order_release) % m_threads.size();

            thread_data& td = m_threads[next_index];

            //add job to queue
            {
                //synchronized operation
                std::lock_guard lock(td.mutex);

                //allocate memory
                void* mem = td.memory_pool.allocate(sizeof(job_type));

                //init the job
                job_type *j = new (mem) job_type(&td, std::move(job));

                //add the job to the queue
                td.jobs.push_back(job_pointer(j));
            }

            //notify the waiting thread
            td.cond.notify_one();
        }

    private:
        //type of used memory pool
        using memory_pool_type = std::pmr::unsynchronized_pool_resource;

        //status
        enum status {
            terminated,
            succeeded,
            failed
        };

        struct thread_data;

        //a job
        struct job_base {
            thread_data* const owner;
            const size_t size;
            job_base(thread_data* o, size_t s) : owner(o), size(s) {}
            virtual ~job_base() {}
            virtual void execute() = 0;
        };

        //a specific job
        template <class J> struct job_impl : job_base {
            const J job;
            job_impl(thread_data* o, J&& j) : job_base(o, sizeof(*this)), job(std::move(j)) {}
            void execute() override final { job(); }
        };

        //job pointer
        using job_pointer = job_base*;

        //thread data
        struct thread_data {
            //mutex that protects the thread data
            std::mutex mutex;

            //memory pool
            memory_pool_type memory_pool;

            //condition variable for the waiting on the queue
            std::condition_variable cond;

            //queue of jobs
            std::deque<job_pointer> jobs;

            //actual thread
            std::thread thread;
        };

        //next available thread
        std::atomic<std::size_t> m_next_index;

        //threads
        std::vector<thread_data> m_threads;

        //stop flag
        std::atomic<bool> m_stop{ false };

        //runs thread on specific thread data
        void run(size_t thread_index);

        //executes next jobs
        status execute_jobs(thread_data& td);

        //steal jobs from other threads
        bool steal_jobs(size_t thread_index);

        //steal jobs from other thread
        bool steal_jobs(thread_data& dst, thread_data& src);

        //executes a job
        static void execute_job(job_base* job);

        //deletes a job
        static void delete_job(job_base* job);
    };


} //namespace execlib


#endif //EXECLIB_JOB_EXECUTOR_HPP
