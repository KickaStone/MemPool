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
                    // v.push_back(malloc(16));
                    v.push_back(malloc((16 + i) % 4096 + 1));// 每一次申请不同桶中的块
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

    printf("%zu threads || %zu rounds || %zu malloc : cost %zu ms\n", nworks, rounds, ntimes, 1000* malloc_costtime.load() / CLOCKS_PER_SEC );
    printf("%zu threads || %zu rounds || %zu free : cost %zu ms\n", nworks, rounds, ntimes, 1000* free_costtime.load() / CLOCKS_PER_SEC );
    printf("%zu threads || %zu rounds || %zu malloc&free : cost %zu ms\n", nworks, rounds, ntimes, 1000* (malloc_costtime.load() + free_costtime.load()) / CLOCKS_PER_SEC );

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
                    // v.push_back(ConcurrentAlloc(16));
                    v.push_back(ConcurrentAlloc((16 + i) % 4096 + 1));
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

    printf("%zu threads || %zu rounds || %zu ConcurrentAlloc : cost %zu ms\n", nworks, rounds, ntimes, 1000* malloc_costtime.load() / CLOCKS_PER_SEC );
    printf("%zu threads || %zu rounds || %zu ConcurrentFree : cost %zu ms\n", nworks, rounds, ntimes, 1000* free_costtime.load() / CLOCKS_PER_SEC );
    printf("%zu threads || %zu rounds || %zu ConcurrentAlloc&ConcurrentFree : cost %zu ms\n", nworks, rounds, ntimes, 1000* (malloc_costtime.load() + free_costtime.load()) / CLOCKS_PER_SEC );
    return malloc_costtime.load() + free_costtime.load();
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        cout << "Usage: " << argv[0] << " <ntimes> <nworks> <rounds> <enable_malloc>" << endl;
        return 1;
    }

    size_t ntimes = atoi(argv[1]);
    size_t nworks = atoi(argv[2]);
    size_t rounds = atoi(argv[3]);
    bool enable_malloc = atoi(argv[4]);

    cout << "================================================" << endl;
    long long malloc_costtime = enable_malloc ? BenchmarkMalloc(ntimes, nworks, rounds) : 0;
    cout << endl
         << endl;

    long long concurrent_costtime = BenchmarkConcurrentAlloc(ntimes, nworks, rounds);

    cout << "================================================" << endl;

    // cout << "ConcurrentAlloc is " << (double)malloc_costtime / (double)concurrent_costtime << " times faster than malloc" << endl;

    return 0;
}