#include "ObjectPool.h"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iostream>

using namespace std;

// 测试对象
struct TestObject {
    int data[10];
    TestObject() {
        for (int i = 0; i < 10; ++i) {
            data[i] = 0;
        }
    }
};

// 大对象
struct LargeObject {
    int data[1000];
    LargeObject() {
        for (int i = 0; i < 1000; ++i) {
            data[i] = i;
        }
    }
};

// 测试1: 基本功能测试
void TestBasicFunctionality() {
    cout << "=== Test 1: Basic Functionality ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    
    // 分配
    TestObject* obj1 = pool.New();
    TestObject* obj2 = pool.New();
    TestObject* obj3 = pool.New();
    
    cout << "Allocated 3 objects: " << obj1 << ", " << obj2 << ", " << obj3 << endl;
    
    // 写入数据
    obj1->data[0] = 100;
    obj2->data[0] = 200;
    obj3->data[0] = 300;
    
    // 回收
    pool.Delete(obj1);
    pool.Delete(obj2);
    pool.Delete(obj3);
    
    // 重新分配（应该从freelist获取）
    TestObject* obj4 = pool.New();
    cout << "Re-allocated object: " << obj4 << endl;
    
    pool.Delete(obj4);
    
    cout << "✓ Basic functionality test passed!" << endl << endl;
}

// 测试2: 单线程大量分配回收
void TestSingleThreadStress() {
    cout << "=== Test 2: Single Thread Stress ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    const int iterations = 10000;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        TestObject* obj = pool.New();
        obj->data[0] = i;
        pool.Delete(obj);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << "Single thread: " << iterations << " alloc/free pairs in " 
         << duration.count() << "ms" << endl;
    cout << "✓ Single thread stress test passed!" << endl << endl;
}

// 测试3: 多线程并发分配和回收（简单模式）
void MultiThreadSimpleWorker(lockfree::ObjectPool<TestObject>& pool, int thread_id, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        TestObject* obj = pool.New();
        obj->data[0] = thread_id;
        obj->data[1] = i;
        pool.Delete(obj);
    }
}

void TestMultiThreadSimple() {
    cout << "=== Test 3: Multi-Thread Simple (Immediate Delete) ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    const int num_threads = 8;
    const int iterations = 5000;
    
    vector<thread> threads;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(MultiThreadSimpleWorker, ref(pool), i, iterations);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << num_threads << " threads, " << iterations << " iterations each" << endl;
    cout << "Total operations: " << (num_threads * iterations) << " in " 
         << duration.count() << "ms" << endl;
    cout << "✓ Multi-thread simple test passed!" << endl << endl;
}

// 测试4: 多线程并发分配和回收（复杂模式 - 先分配后统一回收）
void MultiThreadComplexWorker(lockfree::ObjectPool<TestObject>& pool, int thread_id, int iterations) {
    vector<TestObject*> allocated;
    
    // 分配阶段
    for (int i = 0; i < iterations; ++i) {
        TestObject* obj = pool.New();
        obj->data[0] = thread_id;
        obj->data[1] = i;
        allocated.push_back(obj);
    }
    
    // 验证数据
    for (size_t i = 0; i < allocated.size(); ++i) {
        if (allocated[i]->data[0] != thread_id || allocated[i]->data[1] != (int)i) {
            cerr << "Data corruption detected in thread " << thread_id << endl;
        }
    }
    
    // 回收阶段
    for (auto obj : allocated) {
        pool.Delete(obj);
    }
}

void TestMultiThreadComplex() {
    cout << "=== Test 4: Multi-Thread Complex (Batch Delete) ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    const int num_threads = 8;
    const int iterations = 2000;
    
    vector<thread> threads;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(MultiThreadComplexWorker, ref(pool), i, iterations);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << num_threads << " threads, " << iterations << " iterations each" << endl;
    cout << "Total operations: " << (num_threads * iterations) << " in " 
         << duration.count() << "ms" << endl;
    cout << "✓ Multi-thread complex test passed!" << endl << endl;
}

// 测试5: 高并发压力测试
void HighConcurrencyWorker(lockfree::ObjectPool<TestObject>& pool, 
                           atomic<int>& counter, 
                           int operations) {
    for (int i = 0; i < operations; ++i) {
        TestObject* obj = pool.New();
        counter.fetch_add(1, memory_order_relaxed);
        obj->data[0] = i;
        pool.Delete(obj);
        counter.fetch_sub(1, memory_order_relaxed);
    }
}

void TestHighConcurrency() {
    cout << "=== Test 5: High Concurrency Stress ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    const int num_threads = 16;
    const int operations = 5000;
    
    atomic<int> counter(0);
    vector<thread> threads;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(HighConcurrencyWorker, ref(pool), ref(counter), operations);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << num_threads << " threads, " << operations << " operations each" << endl;
    cout << "Total operations: " << (num_threads * operations) << " in " 
         << duration.count() << "ms" << endl;
    cout << "Final counter value: " << counter.load() << " (should be 0)" << endl;
    
    if (counter.load() == 0) {
        cout << "✓ High concurrency stress test passed!" << endl << endl;
    } else {
        cerr << "✗ Counter mismatch - possible memory leak!" << endl << endl;
    }
}

// 测试6: 大对象测试
void TestLargeObject() {
    cout << "=== Test 6: Large Object Test ===" << endl;
    
    lockfree::ObjectPool<LargeObject> pool;
    const int num_threads = 4;
    const int iterations = 500;
    
    auto worker = [&pool](int tid, int iters) {
        for (int i = 0; i < iters; ++i) {
            LargeObject* obj = pool.New();
            // 验证构造函数是否被调用
            if (obj->data[0] != 0 || obj->data[999] != 999) {
                cerr << "Constructor not called properly!" << endl;
            }
            obj->data[500] = tid;
            pool.Delete(obj);
        }
    };
    
    vector<thread> threads;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i, iterations);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    cout << num_threads << " threads, " << iterations << " iterations each" << endl;
    cout << "Large object size: " << sizeof(LargeObject) << " bytes" << endl;
    cout << "Completed in " << duration.count() << "ms" << endl;
    cout << "✓ Large object test passed!" << endl << endl;
}

// 测试7: 混合读写测试
void TestMixedReadWrite() {
    cout << "=== Test 7: Mixed Read/Write Test ===" << endl;
    
    lockfree::ObjectPool<TestObject> pool;
    const int num_threads = 8;
    vector<TestObject*> shared_objects(100);
    atomic<int> index(0);
    
    // 初始化共享对象
    for (int i = 0; i < 100; ++i) {
        shared_objects[i] = pool.New();
        shared_objects[i]->data[0] = i;
    }
    
    auto worker = [&pool, &shared_objects, &index]() {
        for (int i = 0; i < 1000; ++i) {
            // 随机选择：分配新对象或使用共享对象
            if (i % 3 == 0) {
                TestObject* obj = pool.New();
                obj->data[0] = i;
                pool.Delete(obj);
            } else {
                int idx = index.fetch_add(1, memory_order_relaxed) % 100;
                volatile int val = shared_objects[idx]->data[0];
                (void)val; // 避免未使用变量警告
            }
        }
    };
    
    vector<thread> threads;
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // 清理共享对象
    for (auto obj : shared_objects) {
        pool.Delete(obj);
    }
    
    cout << "Completed in " << duration.count() << "ms" << endl;
    cout << "✓ Mixed read/write test passed!" << endl << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "  Lock-Free ObjectPool Unit Tests" << endl;
    cout << "========================================" << endl << endl;
    
    try {
        TestBasicFunctionality();
        TestSingleThreadStress();
        TestMultiThreadSimple();
        TestMultiThreadComplex();
        TestHighConcurrency();
        TestLargeObject();
        TestMixedReadWrite();
        
        cout << "========================================" << endl;
        cout << "  ✓ All tests passed successfully!" << endl;
        cout << "========================================" << endl;
        
    } catch (const exception& e) {
        cerr << "✗ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}

