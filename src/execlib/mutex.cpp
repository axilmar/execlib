#include <set>
#include <algorithm>
#include <memory_resource>
#include "execlib/mutex.hpp"


namespace execlib {


    using allocator = std::pmr::polymorphic_allocator<mutex*>;
    using locked_mutex_table = std::multiset<mutex*, std::less<mutex*>, allocator>;


    //combines an unsynchronized memory pool and a table
    struct local_thread_resources {
        std::pmr::unsynchronized_pool_resource memory_pool;
        locked_mutex_table table;
        local_thread_resources() : table(allocator(&memory_pool)) {}
    };


    //get the locked mutex table; safely initialized on first call
    static locked_mutex_table& get_locked_mutex_table() {
        static thread_local local_thread_resources r;
        return r.table;
    }


    //try to lock the mutex
    bool mutex::try_lock() {
        auto& lmt = get_locked_mutex_table();

        //insert the mutex in the thread's locked mutex table
        auto it = lmt.insert(this);

        //if the mutex was successfully locked, do nothing else
        if (m_mutex.try_lock()) {
            return true;
        }

        //the mutex could not be locked, because another thread has locked it;
        //we don't know if there is a deadlock, so unlock all mutexes above this,
        //then lock this mutex and then lock all mutexes above this

        //unlock all mutexes above this
        for (auto it1 = std::next(it); it1 != lmt.end(); ++it1) {
            (*it1)->m_mutex.unlock();
        }

        //try to lock this; on failure to lock this, 
        //remove the lock entry and relock all mutexes above this
        if (!m_mutex.try_lock()) {
            for (auto it1 = std::next(it); it1 != lmt.end(); ++it1) {
                (*it1)->m_mutex.lock();
            }
            lmt.erase(it);
            return false;
        }

        //lock all mutexes above this
        for (auto it1 = std::next(it); it1 != lmt.end(); ++it1) {
            (*it1)->m_mutex.lock();
        }

        //success
        return true;
    }


    //lock the mutex
    void mutex::lock() {
        auto& lmt = get_locked_mutex_table();

        //insert the mutex in the thread's locked mutex table
        auto it = lmt.insert(this);

        //if the mutex was successfully locked, do nothing else
        if (m_mutex.try_lock()) {
            return;
        }

        //the mutex could not be locked, because another thread has locked it;
        //we don't know if there is a deadlock, so unlock all mutexes above this,
        //then lock this mutex and then lock all mutexes above this

        //unlock all mutexes above this
        for (auto it1 = std::next(it); it1 != lmt.end(); ++it1) {
            (*it1)->m_mutex.unlock();
        }

        //lock all mutexes from this and above this
        for (auto it1 = it; it1 != lmt.end(); ++it1) {
            (*it1)->m_mutex.lock();
        }
    }


    //unlock the mutex
    void mutex::unlock() {
        m_mutex.unlock();
        get_locked_mutex_table().erase(this);
    }


} //namespace execlib
