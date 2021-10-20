#ifndef EXECLIB_EXECUTOR_INTERNALS_HPP
#define EXECLIB_EXECUTOR_INTERNALS_HPP


namespace execlib {


    class executor;


    //executor internals
    class executor_internals {
    private:
        //interface for jobs.
        class job {
        public:
            //constructor
            job(size_t size) : m_size(size) {}

            //destructor is virtual due to polymorphism
            virtual ~job() {}

            //get size
            size_t get_size() const { return m_size; }

            //executes the job
            virtual void invoke() = 0;

        private:
            const size_t m_size;
        };

        //job implementation.
        template <class F> class job_impl : public job {
        public:
            //constructor
            job_impl(F&& f) : job(sizeof(job_impl<F>)), m_function(f) {}

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
