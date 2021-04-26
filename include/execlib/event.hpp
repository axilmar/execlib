#ifndef EXECLIB_EVENT_HPP
#define EXECLIB_EVENT_HPP


#include <mutex>
#include <condition_variable>


namespace execlib {


    /**
     * An event class represents a boolean value that is set to true when something happens.
     * It remains so until cleared.
     */
    class event {
    public:
        /**
         * The constructor.
         * @param value initial value.
         */
        event(bool value = false) : m_value(value) {
        }

        /**
         * Sets the value to true and notifies one thread.
         */
        void set_and_notify_one() {
            m_mutex.lock();
            m_value = true;
            m_mutex.unlock();
            m_cond.notify_one();
        }

        /**
         * Sets the value to true and notifies all threads.
         */
        void set_and_notify_all() {
            m_mutex.lock();
            m_value = true;
            m_mutex.unlock();
            m_cond.notify_all();
        }

        /**
         * Waits for the value to become true, then it sets the value to false.
         */
        void wait() {
            std::unique_lock lock(m_mutex);
            while (!m_value) {
                m_cond.wait(lock);
            }
            m_value = false;
        }

    private:
        std::mutex m_mutex;
        bool m_value;
        std::condition_variable m_cond;
    };


} //namespace execlib


#endif //EXECLIB_EVENT_HPP
