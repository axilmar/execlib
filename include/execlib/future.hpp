#ifndef EXECLIB_FUTURE_HPP
#define EXECLIB_FUTURE_HPP


#include <mutex>
#include <condition_variable>


namespace execlib {


    /**
     * A future represents a future value.
     * Unlike c++'s futures, this is a single object, no promise object is required.
     * Thus, this object shall remain in scope until the result is available.
     * @param T type of value.
     */
    template <class T> class future {
    public:
        /**
         * The constructor from default value.
         */
        future() : m_value{} {
        }

        /**
         * The constructor from value.
         * @param src source value.
         */
        template <class U> future(U&& src) : m_value(std::forward<U>(src)) {
        }

        /**
         * Sets the value and notifies one thread.
         * @param src source value.
         */
        template <class U> void set_and_notify_one(U&& src) {
            m_mutex.lock();
            m_value = std::forward<U>(src);
            m_set = true;
            m_mutex.unlock();
            m_cond.notify_one();
        }

        /**
         * Sets the value and notifies all threads.
         */
        template <class U> void set_and_notify_all(U&& src) {
            m_mutex.lock();
            m_value = std::forward<U>(src);
            m_set = true;
            m_mutex.unlock();
            m_cond.notify_all();
        }

        /**
         * Waits for the value to be set, then it returns the value.
         * @return the value.
         */
        const T& wait() {
            std::unique_lock lock(m_mutex);
            while (!m_set) {
                m_cond.wait(lock);
            }
            m_set = false;
            return m_value;
        }

    private:
        std::mutex m_mutex;
        T m_value;
        bool m_set{ false };
        std::condition_variable m_cond;
    };


} //namespace execlib


#endif //EXECLIB_FUTURE_HPP
