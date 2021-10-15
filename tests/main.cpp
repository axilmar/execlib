#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include "execlib.hpp"


static execlib::executor executor;


template <class F> double time_func(F&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
}


static std::vector<std::string> prepare_test_data() {
    std::random_device rd;
    std::default_random_engine re(rd());
    std::uniform_int_distribution<int> dist(' ', '~');

    std::vector<std::string> result;

    for (size_t i = 0; i < std::thread::hardware_concurrency()*100; ++i) {
        result.emplace_back();
        for (size_t j = 0; j < 8; ++j) {
            result.back().push_back((char)dist(re));
        }
    }

    return result;
}


static void create_all_combinations(const std::string& in_str, std::string& out_str, size_t position = 0) {
    if (position < in_str.size()) {
        for (size_t i = 0; i < in_str.size(); ++i) {
            out_str[position] = in_str[i];
            create_all_combinations(in_str, out_str,position + 1);
        }
    }
}


static void test() {
    const auto test_data = prepare_test_data();
    const size_t partition_size = test_data.size() / executor.thread_count();
    const size_t partition_count = (test_data.size() + partition_size - 1) / partition_size;

    //std::vector<execlib::future<bool>> events(partition_count);
    execlib::counter<int> counter;

    for (size_t partition_index = 0; partition_index < partition_count; ++partition_index) {
        const size_t partition_start = partition_index * partition_size;
        const size_t partition_end = partition_index < partition_count - 1 ? partition_start + partition_size : test_data.size();

        counter.increment_and_notify_one();
        executor.execute([partition_test_data = std::vector<std::string>(test_data.begin() + partition_start, test_data.begin() + partition_end), pi = partition_index/*, &events*/, &counter]() {
            for (size_t index = 0; index < partition_test_data.size(); ++index) {
                const std::string in_str = partition_test_data[index];
                std::string out_str(in_str);
                create_all_combinations(in_str, out_str);
            }
            //events[pi].set_and_notify_one(true);
            counter.decrement_and_notify_one();
        });
    }

    /*
    for (execlib::future<bool>& e : events) {
        e.wait();
    }
    */
    counter.wait();
}


static void performance_test() {
    double d = time_func([&]() { test(); });
    std::cout << d << std::endl;
}


const size_t MUTEX_TEST_COUNT = 10000;
using test_mutex = execlib::mutex;
//using test_mutex = std::mutex;
test_mutex mutexA;
test_mutex mutexB;


static void thread1_proc() {
    for (size_t i = 0; i < MUTEX_TEST_COUNT; ++i) {
        std::lock_guard lockMutexA(mutexA);
        std::lock_guard lockMutexB(mutexB);
        std::cout << "thread 1 step: " << i << std::endl;
    }
}


static void thread2_proc() {
    for (size_t i = 0; i < MUTEX_TEST_COUNT; ++i) {
        std::lock_guard lockMutexB(mutexB);
        std::lock_guard lockMutexA(mutexA);
        std::cout << "thread 2 step: " << i << std::endl;
    }
}


static void mutex_test() {
    std::thread thread1{ thread1_proc };
    std::thread thread2{ thread2_proc };
    thread1.join();
    thread2.join();
}


int main() {
    //performance_test();
    mutex_test();
    system("pause");
    return 0;
}
