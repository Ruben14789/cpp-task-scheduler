
#include "thread_pool.h"    //header file
#include <chrono>           //for std::chrono parallel schediling
#include <iostream>         //For cout
#include <vector>           //to use vectors


int main() {
    // Create a thread pool with 4 worker threads
    ThreadPool pool(4);

    // Vector to store futures returned by submitted tasks
    std::vector<std::future<int>> results;

    // Submit 8 tasks to the thread pool
    // Each task sleeps for 100ms and returns i * i
    for (int i = 0; i < 8; ++i) {
        results.push_back(pool.enqueue([i] {
            // Simulate work parallel scheduling //
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Return results
            return i * i;
        }));
    }

    // Retrieve results from all futures
    // .get() blocks until each task is complete
    // Results are printed in submission order, not completion order
    for (std::size_t i = 0; i < results.size(); ++i) {
        //print out the results
        std::cout << "task " << i << " -> " << results[i].get() << '\n';
    }

    // ThreadPool destructor
    // all worker threads are properly stopped
    return 0;
}