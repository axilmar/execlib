#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include "execlib.hpp"


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


static void test(execlib::executor& executor) {
    const auto test_data = prepare_test_data();
    const size_t partition_size = test_data.size() / executor.thread_count();
    const size_t partition_count = (test_data.size() + partition_size - 1) / partition_size;

    //std::vector<execlib::future<bool>> events(partition_count);
    execlib::counter<int> counter;

    for (size_t partition_index = 0; partition_index < partition_count; ++partition_index) {
        const size_t partition_start = partition_index * partition_size;
        const size_t partition_end = partition_index < partition_count - 1 ? partition_start + partition_size : test_data.size();

        counter.increment_and_notify_one();
        executor.execute([partition_test_data = std::vector<std::string>(test_data.begin() + partition_start, test_data.begin() + partition_end), pi = partition_index/*, &events*/, &counter, partition_start, partition_end]() {
            printf("partioning data from %zi to %zi\n", partition_start, partition_end);
            for (size_t index = 0; index < partition_test_data.size(); ++index) {
                const std::string& in_str = partition_test_data[index];
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
    static execlib::executor executor;
    double d = time_func([&]() { test(executor); });
    std::cout << d << std::endl;
}


using test_mutex = execlib::deadlock_free_mutex;
//using test_mutex = std::mutex;


static void thread1_proc(const size_t test_count, test_mutex& mutexA, test_mutex& mutexB) {
    for (size_t i = 0; i < test_count; ++i) {
        {
            std::lock_guard lockMutexA(mutexA);
            std::lock_guard lockMutexB(mutexB);
            printf("thread 1 step: %zi\n", i);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds((int)((rand() / (double)RAND_MAX)*100)));
    }
}


static void thread2_proc(const size_t test_count, test_mutex& mutexA, test_mutex& mutexB) {
    for (size_t i = 0; i < test_count; ++i) {
        {
            std::lock_guard lockMutexB(mutexB);
            std::lock_guard lockMutexA(mutexA);
            printf("thread 2 step: %zi\n", i);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds((int)((rand() / (double)RAND_MAX) * 100)));
    }
}


static void mutex_test() {
    const size_t MUTEX_TEST_COUNT = 10000;
    test_mutex mutexA;
    test_mutex mutexB;
    std::thread thread1{ thread1_proc, MUTEX_TEST_COUNT, std::ref(mutexA), std::ref(mutexB) };
    std::thread thread2{ thread2_proc, MUTEX_TEST_COUNT, std::ref(mutexA), std::ref(mutexB) };
    thread1.join();
    thread2.join();
}


static void release_worker_thread_test() {
    execlib::executor executor(1);
    execlib::counter counter(2);

    executor.execute([&]() {
        execlib::executor::release_current_worker_thread();
        printf("1st job started\n");
        size_t sum = 0;
        for (size_t i = 0; i < 1000; ++i) {
            printf("added %zi\n", i);
            sum += i;
        }
        printf("sum = %zi\n", sum);
        printf("1st job ended\n");
        counter.decrement_and_notify_one();
    });

    executor.execute([&]() {
        printf("2nd job started\n");
        printf("2nd job ended\n");
        counter.decrement_and_notify_one();
    });

    counter.wait();
}


int main() {
    performance_test();
    release_worker_thread_test();
    mutex_test();
    system("pause");
    return 0;
}
