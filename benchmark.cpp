/**
 * benchmark for mempool
 *
 */

#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "ConcurrentAlloc.h"

using std::cout;
using std::endl;

long long BenchmarkMalloc(int ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime(0);
    std::atomic<size_t> free_costtime(0);

    // nworks
    for (size_t k = 0; k < nworks; ++k)
    {
        vthread[k] = std::thread([&, k]()
                                 {
            std::vector<void*> v;
            v.reserve(ntimes);

            for(size_t i = 0; i < rounds; ++i){
                size_t begin1 = clock();

                for(size_t j = 0; j < ntimes; ++j){
                    v.push_back(malloc(16));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for(size_t j = 0; j < ntimes; ++j){
                    free(v[j]);
                }

                size_t end2 = clock();

                malloc_costtime += end1 - begin1;
                free_costtime += end2 - begin2;
                
                // Clear the vector after each round
                v.clear();
            } });
    }

    for (auto &t : vthread)
    {
        t.join();
    }

    printf("%zu threads || %zu rounds || %zu malloc : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load());
    printf("%zu threads || %zu rounds || %zu free : cost %zu ms\n", nworks, rounds, ntimes, free_costtime.load());
    printf("%zu threads || %zu rounds || %zu malloc&free : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load() + free_costtime.load());

    return malloc_costtime.load() + free_costtime.load();
}

long long BenchmarkConcurrentAlloc(int ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime(0);
    std::atomic<size_t> free_costtime(0);

    // nworks
    for (size_t k = 0; k < nworks; ++k)
    {
        vthread[k] = std::thread([&, k]()
                                 {
            std::vector<void*> v;
            v.reserve(ntimes);

            for(size_t i = 0; i < rounds; ++i){
                size_t begin1 = clock();

                for(size_t j = 0; j < ntimes; ++j){
                    v.push_back(ConcurrentAlloc(16));
                }
                size_t end1 = clock();
                size_t begin2 = clock();
                for(size_t j = 0; j < ntimes; ++j){
                    ConcurrentFree(v[j]);
                }

                size_t end2 = clock();

                malloc_costtime += end1 - begin1;
                free_costtime += end2 - begin2;
                
                // Clear the vector after each round
                v.clear();
            } });
    }

    for (auto &t : vthread)
    {
        t.join();
    }

    printf("%zu threads || %zu rounds || %zu ConcurrentAlloc : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load());
    printf("%zu threads || %zu rounds || %zu ConcurrentFree : cost %zu ms\n", nworks, rounds, ntimes, free_costtime.load());
    printf("%zu threads || %zu rounds || %zu ConcurrentAlloc&ConcurrentFree : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load() + free_costtime.load());
    return malloc_costtime.load() + free_costtime.load();
}

int main(int argc, char *argv[])
{
    // if (argc != 4)
    // {
    //     cout << "Usage: " << argv[0] << " <ntimes> <nworks> <rounds>" << endl;
    //     return 1;
    // }

    // size_t ntimes = atoi(argv[1]);
    // size_t nworks = atoi(argv[2]);
    // size_t rounds = atoi(argv[3]);

    size_t ntimes = 50000;
    size_t nworks = 4;
    size_t rounds = 1000;
    cout << "================================================" << endl;
    // long long malloc_costtime = BenchmarkMalloc(ntimes, nworks, rounds);
    // cout << endl
    //      << endl;

    long long concurrent_costtime = BenchmarkConcurrentAlloc(ntimes, nworks, rounds);

    cout << "================================================" << endl;

    // cout << "ConcurrentAlloc is " << (double)malloc_costtime / (double)concurrent_costtime << " times faster than malloc" << endl;

    return 0;
}