#ifndef EXECLIB_COUNTER_HPP
#define EXECLIB_COUNTER_HPP


#include <atomic>
#include <mutex>
#include <condition_variable>


namespace execlib {


    /**
     * The default predicate for counter.
     * @param T type of counter value.
     */
    template <class T> struct is_counter_zero {
        /**
         * Tests if the counter is zero.
         * @param value current value.
         * @return true if zero, false otherwise.
         */
        bool operator ()(T value) const {
            return value == (T)0;
        }
    };


    /**
     * Predicate that tests if a counter equals a specific value.
     * @param T type of counter value.
     */
    template <class T> struct is_counter_equal_to {
        const T value;

        /**
         * Constructor.
         * @param v value to compare to counter.
         */
        is_counter_equal_to(T v) : value(v) {
        }

        /**
         * Tests if the counter equals a specific value.
         * @param value current value.
         * @return true if equals a value, false otherwise.
         */
        bool operator ()(T value) const {
            return value == this.value;
        }
    };


    /**
     * A synchronized counter class.
     * It can be used to implement waiting for tasks when the counter reaches a specific value.
     * @param T type of counter value.
     * @param P type of predicate to use for testing if an event should be reported.
     */
    template <class T, class P = is_counter_zero<T>> class counter {
    public:
        /**
         * The default constructor.
         * @param initial_value initial value.
         */
        counter(T&& initial_value = T()) : 
            m_value(std::move(initial_value))
        {
        }

        /**
         * The constructor from value and predicate.
         * @param initial_value initial value.
         * @param pred predicate.
         */
        counter(T&& initial_value, P&& pred) :
            m_value(std::move(initial_value)),
            m_pred(std::move(pred))
        {
        }

        /**
         * Returns the value.
         * @return the value.
         */
        operator T () const {
            return m_value;
        }

        /**
         * Returns the value.
         * @return the value.
         */
        T get() const {
            return m_value;
        }

        /**
         * Atomically increments the counter.
         */
        void increment() {
            m_value.fetch_add((T)1, std::memory_order_release);
        }

        /**
         * Atomically decrements the counter.
         */
        void decrement() {
            m_value.fetch_sub((T)1, std::memory_order_release);
        }

        /**
         * Atomically increments the counter.
         * It notifies one thread if the predicate returns true.
         */
        void increment_and_notify_one() {
            const T new_value = m_value.fetch_add((T)1, std::memory_order_release) + (T)1;
            if (m_pred(new_value)) {
                m_cond.notify_one();
            }
        }

        /**
         * Atomically decrements the counter.
         * It notifies one thread if the predicate returns true.
         */
        void decrement_and_notify_one() {
            const T new_value = m_value.fetch_sub((T)1, std::memory_order_release) - (T)1;
            if (m_pred(new_value)) {
                m_cond.notify_one();
            }
        }

        /**
         * Atomically increments the counter.
         * It notifies all threads if the predicate returns true.
         */
        void increment_and_notify_all() {
            const T new_value = m_value.fetch_add((T)1, std::memory_order_release) + (T)1;
            if (m_pred(new_value)) {
                m_cond.notify_all();
            }
        }

        /**
         * Atomically decrements the counter.
         * It notifies all threads.
         */
        void decrement_and_notify_all() {
            const T new_value = m_value.fetch_sub((T)1, std::memory_order_release) - (T)1;
            if (m_pred(new_value)) {
                m_cond.notify_all();
            }
        }

        /**
         * Atomically waits for the counter to get to a specific value,
         * depending on the predicate.
         */
        void wait() {
            while (!m_pred(m_value.load(std::memory_order_acquire))) {
                std::unique_lock lock(m_mutex);
                m_cond.wait(lock);
            }
        }

    private:
        std::atomic<T> m_value;
        P m_pred;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    };


} //namespace execlib


#endif //EXECLIB_COUNTER_HPP
