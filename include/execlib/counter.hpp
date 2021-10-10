#ifndef EXECLIB_COUNTER_HPP
#define EXECLIB_COUNTER_HPP


#include <mutex>
#include <condition_variable>


namespace execlib {


    /**
     * A synchronized counter class.
     * It can be used to implement waiting for tasks to finish.
     */
    template <class T> class counter {
    public:
        /**
         * The default constructor.
         * @param initial_value initial value.
         */
        counter(T&& initial_value = T()) : m_value(std::move(initial_value)) {
        }

        /**
         * Atomically increments the counter.
         */
        void increment() {
            std::lock_guard lock(m_mutex);
            ++m_value;
        }

        /**
         * Atomically decrements the counter.
         */
        void decrement() {
            std::lock_guard lock(m_mutex);
            --m_value;
        }

        /**
         * Atomically increments the counter.
         * It notifies one thread.
         */
        void increment_and_notify_one() {
            {
                std::lock_guard lock(m_mutex);
                ++m_value;
            }
            m_cond.notify_one();
        }

        /**
         * Atomically decrements the counter.
         * It notifies one thread.
         */
        void decrement_and_notify_one() {
            {
                std::lock_guard lock(m_mutex);
                --m_value;
            }
            m_cond.notify_one();
        }

        /**
         * Atomically increments the counter.
         * It notifies all threads.
         */
        void increment_and_notify_all() {
            {
                std::lock_guard lock(m_mutex);
                ++m_value;
            }
            m_cond.notify_all();
        }

        /**
         * Atomically decrements the counter.
         * It notifies all threads.
         */
        void decrement_and_notify_all() {
            {
                std::lock_guard lock(m_mutex);
                --m_value;
            }
            m_cond.notify_all();
        }

        /**
         * Atomically waits for the counter to get to a specific value.
         * The function blocks as long as the predicate is false.
         * @param pred predicate; it should be of type bool(T value),
         *  and it shall return true if the condition is satisfied.
         */
        template <class P> void wait(P&& pred) {
            std::unique_lock lock(m_mutex);
            while (!pred(m_value)) {
                m_cond.wait(lock);
            }
        }

        /**
         * Atomically waits for the value to become zero.
         */
        void wait() {
            return wait([](const T& value) { return value == (T)0;  });
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_cond;
        T m_value;
    };


} //namespace execlib


#endif //EXECLIB_COUNTER_HPP
