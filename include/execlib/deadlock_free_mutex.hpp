#ifndef EXECLIB_DEADLOCK_FREE_MUTEX_HPP
#define EXECLIB_DEADLOCK_FREE_MUTEX_HPP


#include <mutex>


namespace execlib {


    /**
     * A deadlock-free mutex.
     * 
     * It achieves deadlock avoidance by unlocking and then relocking all mutexes
     * locked by this thread and that are above this, in memory order.
     * 
     */
    class deadlock_free_mutex {
    public:
        /**
         * Tries to lock the mutex.
         */
        bool try_lock();

        /**
         * Locks the mutex.
         */
        void lock();

        /**
         * Unlocks the mutex.
         */
        void unlock();

    private:
        //the mutex
        std::recursive_mutex m_mutex;
    };


} //namespace execlib


#endif //EXECLIB_DEADLOCK_FREE_MUTEX_HPP
