#include <set>
#include <algorithm>
#include <memory_resource>
#include "execlib/deadlock_free_mutex.hpp"


namespace execlib {


    using allocator = std::pmr::polymorphic_allocator<deadlock_free_mutex*>;
    using locked_mutex_table = std::multiset<deadlock_free_mutex*, std::less<deadlock_free_mutex*>, allocator>;


    //combines an unsynchronized memory pool and a table;
    //allocation is thread-local and therefore there is no need
    //for synchronization
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
    bool deadlock_free_mutex::try_lock() {
        //if the mutex is successfully locked,
        //insert the mutex in the thread's locked mutex table
        //and return success
        if (m_mutex.try_lock()) {
            get_locked_mutex_table().insert(this);
            return true;
        }

        //failed to lock the mutex
        return false;
    }


    //lock the mutex
    void deadlock_free_mutex::lock() {
        auto& lmt = get_locked_mutex_table();

        //if the mutex is successfully locked,
        //insert the mutex in the thread's locked mutex table
        //and return
        if (m_mutex.try_lock()) {
            lmt.insert(this);
            return;
        }

        //the mutex could not be locked, maybe due to deadlock;
        //unlock and relock all mutexes above this in memory order
       
        //insert the mutex into the locked mutex table in order to find
        //all the mutexes above this mutex
        const auto it = lmt.insert(this);

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
    void deadlock_free_mutex::unlock() {
        m_mutex.unlock();
        get_locked_mutex_table().erase(this);
    }


} //namespace execlib
