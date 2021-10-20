#ifndef EXECLIB_EXECUTOR_INTERNALS_PRIVATE_HPP
#define EXECLIB_EXECUTOR_INTERNALS_PRIVATE_HPP


#include <mutex>
#include <memory_resource>
#include "execlib/executor_internals.hpp"


namespace execlib {


    //queue base
    struct executor_internals::queue_base {
        //mutex of queue.
        std::mutex m_mutex;

        //memory pool for queue
        std::pmr::unsynchronized_pool_resource m_memory_pool;
    };


} //namespace execlib


#endif //EXECLIB_EXECUTOR_INTERNALS_PRIVATE_HPP
