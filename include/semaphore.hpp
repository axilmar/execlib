#ifndef EXECLIB_SEMAPHORE_HPP
#define EXECLIB_SEMAPHORE_HPP


#include <mutex>
#include <condition_variable>


namespace execlib {


    /**
     * A semaphore class represents a resource counter.
     */
    class semaphore {
    public:
        /**
         * The constructor.
         * @param value initial value.
         */
        semaphore(const size_t value) : m_value(value) {
        }

        /**
         * Increments the value and notifies one thread.
         * @param value value to change this semaphore by.
         */
        void set_and_notify_one(const size_t value = 1) {
            m_mutex.lock();
            m_value += value;
            m_mutex.unlock();
            m_cond.notify_one();
        }

        /**
         * Increments the value and notifies all threads.
         * @param value value to change this semaphore by.
         */
        void set_and_notify_all(const size_t value = 1) {
            m_mutex.lock();
            m_value += value;
            m_mutex.unlock();
            m_cond.notify_all();
        }

        /**
         * Waits for the value to become greater than 0, then it decrements the value.
         * Use this function to acquire the resource.
         */
        void wait() {
            std::unique_lock lock(m_mutex);
            while (m_value == 0) {
                m_cond.wait(lock);
            }
            --m_value;
        }

        /**
         * Used for acquiring a resource.
         * Invokes 'wait()'.
         */
        void acquire() {
            wait();
        }

        /**
         * Used for releasing a resource.
         * Invokes 'set_and_notify_one(1)'.
         */
        void release() {
            set_and_notify_one();
        }

    private:
        std::mutex m_mutex;
        size_t m_value;
        std::condition_variable m_cond;
    };


} //namespace execlib


#endif //EXECLIB_SEMAPHORE_HPP
