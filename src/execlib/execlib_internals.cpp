#include "execlib/executor_internals.hpp"


namespace execlib {


    //deletes this job.
    void executor_internals::job::delete_this() {
        //get the data needed for deallocation
        const size_t size = m_size;
        queue_base* const queue = m_queue;

        //execute the destructor
        this->~job();

        //deallocate the memory of this synchronized, 
        //because the job may be allocated from another queue,
        //in case of stolen jobs.
        std::lock_guard lock_queue(queue->m_mutex);
        queue->m_memory_pool.deallocate(this, size);
    }


} //namespace execlib
