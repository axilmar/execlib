#include <algorithm>
#include "execlib/executor.hpp"


namespace execlib {


    //get job from queue
    template <class T> static T* queue_get(std::deque<T*>& queue) {
        T* const item = queue.front();
        queue.pop_front();
        return item;
    }


    //The constructor.
    executor::executor(size_t thread_count) : m_threads(thread_count) {
        //check the argument
        if (thread_count == 0) {
            throw std::invalid_argument("thread count is 0");
        }

        //start the threads
        for (size_t index = 0; index < thread_count; ++index) {
            m_threads[index].thread = std::thread([this, index]() { run(index); });
        }
    }


    //stops the execution.
    executor::~executor() {
        //set the stop flag to true in order for threads to terminate
        m_stop.store(true, std::memory_order_release);

        //notify threads of termination
        for (thread_data& td : m_threads) {
            td.cond.notify_one();
        }

        //wait for threads to terminate
        for (thread_data& td : m_threads) {
            td.thread.join();
        }
    }


    //runs thread on specific thread data entry
    void executor::run(size_t thread_index) {
        thread_data& td = m_threads[thread_index];

        job_pointer job;

        for (;;) {
            //try to execute jobs from queue
            switch (execute_jobs(td)) {
                case terminated: return;
            }

            //if there was no job to execute, 
            //steal jobs from other threads
            if (steal_jobs(thread_index)) {
                continue;
            }

            //if no job was available either in this queue 
            //or from other queues, then wait for job
            {
                //synchronized operation
                std::unique_lock lock(td.mutex);

                //if empty, enter wait loop
                while (td.jobs.empty()) {

                    //wait
                    td.cond.wait(lock);

                    //if stop was requested, exit
                    if (m_stop.load(std::memory_order_acquire)) {
                        return;
                    }
                }

                //get job
                job = queue_get(td.jobs);
            }

            //execute job
            execute_job(job);
        }
    }


    //executes next job
    executor::status executor::execute_jobs(thread_data& td) {
        job_pointer job;

        //loop until no more jobs available
        for(;;)
        {
            {
                //synchronized operation
                std::lock_guard lock(td.mutex);

                //if stop was requested during waiting, exit
                if (m_stop.load(std::memory_order_acquire)) {
                    return terminated;
                }

                //if empty queue, continue with job stealing
                if (td.jobs.empty()) {
                    return failed;
                }

                //pop job off the queue
                job = queue_get(td.jobs);
            }

            //execute job
            execute_job(job);
        }

        //unreachable; the above loop goes on as long as 
        //it is not terminated and there are jobs in the queue
        return succeeded;
    }


    //steal jobs from other threads
    bool executor::steal_jobs(size_t thread_index) {
        thread_data& td = m_threads[thread_index];

        //try from next of thread index to end
        for (size_t next_thread_index = thread_index + 1; next_thread_index < m_threads.size(); ++next_thread_index) {
            if (steal_jobs(td, m_threads[next_thread_index])) {
                return true;
            }
        }

        //try from first to thread index
        for (size_t next_thread_index = 0; next_thread_index < thread_index; ++next_thread_index) {
            if (steal_jobs(td, m_threads[next_thread_index])) {
                return true;
            }
        }

        return false;
    }


    //steal jobs from other thread
    bool executor::steal_jobs(thread_data& dst, thread_data& src) {
        //lock both threads
        std::scoped_lock lock(dst.mutex, src.mutex);

        //if the source thread has very few jobs, do nothing
        if (src.jobs.size() < 8) {
            return false;
        }

        const auto start = src.jobs.begin() + src.jobs.size() / 2;
        const auto end = src.jobs.end();

        //copy the newsest half of jobs to destination
        dst.jobs.insert(dst.jobs.end(), start, end);

        //remove jobs from source
        src.jobs.erase(start, end);

        return true;
    }


    //executes a job
    void executor::execute_job(job_base* job) {
        try {
            job->execute();
        }
        catch (...) {
            delete_job(job);
            throw;
        }
        delete_job(job);
    }



    //deletes a job
    void executor::delete_job(job_base* job) {
        thread_data* const td = job->owner;
        const size_t size = job->size;

        //destroy the job
        job->~job_base();

        //deallocation should be synchronized
        std::lock_guard lock(td->mutex);

        //deallocate the job
        td->memory_pool.deallocate(job, size);
    }


} //namespace execlib
