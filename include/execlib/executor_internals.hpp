#ifndef EXECLIB_EXECUTOR_INTERNALS_HPP
#define EXECLIB_EXECUTOR_INTERNALS_HPP


#include <mutex>
#include <memory_resource>


namespace execlib {


    class executor;


    //executor internals
    class executor_internals {
    private:
        //queue base
        struct queue_base {
            //mutex of queue.
            std::mutex m_mutex;

            //memory pool for queue
            std::pmr::unsynchronized_pool_resource m_memory_pool;
        };

        //interface for jobs.
        class job {
        public:
            //constructor
            job(size_t size, queue_base* q) : m_size(size), m_queue(q) {}

            //destructor is virtual due to polymorphism
            virtual ~job() {}

            //executes the job
            virtual void invoke() = 0;

            //deletes this job.
            void delete_this();

        private:
            const size_t m_size;
            queue_base* const m_queue;
        };

        //job implementation.
        template <class F> class job_impl : public job {
        public:
            //constructor
            job_impl(queue_base* q, F&& f) 
                : job(sizeof(job_impl<F>), q), m_function(std::move(f))
            {
            }

            //executes the function
            void invoke() override {
                m_function();
            }

        private:
            //function to execute
            F m_function;
        };

        friend class executor;
    };


} //namespace execlib


#endif //EXECLIB_EXECUTOR_INTERNALS_HPP
