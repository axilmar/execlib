#ifndef EXECLIB_EXECUTION_ENGINE_HPP
#define EXECLIB_EXECUTION_ENGINE_HPP


namespace execlib {


    /**
     * Task interface.
     */
    class task {
    public:
        /**
         * The destructor.
         */
        virtual ~task() {
        }

        /**
         * Interface for executing this task. 
         */
        virtual void execute() const noexcept = 0;
    };


    /**
     * Task implementation that invokes a callable.
     * @param T type of callable.
     */
    template <class T> class callable_task : public task {
    public:
        /**
         * Constructor.
         * @param callable callable to execute.
         */
        callable_task(T&& callable) noexcept : m_callable(std::move(callable)) {
        }

        /**
         * Executes the callable. 
         */
        void execute() const noexcept final {
            m_callable();
        }

    private:
        //callable
        T m_callable;
    };


    /**
     * Initializes the execution engine.
     * If the engine is already initialized, behavior is undefined.
     * @param thread_count number of threads to use.
     * @exception std::invalid_argument thown if the thread count is 0.
     */
    void initialize(const size_t thread_count);


    /**
     * Initializes the execution engine with the number of threads supported by the hardware. 
     */
    void initialize();


    /**
     * Stops the executing threads and cleans up the eecution engine.
     * The engine can then be re-initialized.
     * If the engine is not initialized, behavior is undefined.
     */
    void cleanup();


    /**
     * Executes the given task in parallel.
     * The task is added to the queue of one of the worker threads, in round-robin fashion.
     * If the engine is not initialized, behavior is undefined.
     * @param task task task to execute.
     */
    void execute(task* const t) noexcept;


    /**
     * Executes the given task in parallel.
     * The task is added to the queue of one of the worker threads, in round-robin fashion.
     * If the engine is not initialized, behavior is undefined.
     * @param callable task callable task to execute.
     */
    template <class T> void execute(T&& callable) noexcept {
        execute(new callable_task<T>(std::forward<T>(callable)));
    }


} //namespace execlib


#endif //EXECLIB_EXECUTION_ENGINE_HPP
