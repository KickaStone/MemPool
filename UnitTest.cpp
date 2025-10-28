#include "ConcurrentAlloc.h"
#include <thread>

void Alloc1(){
    // 两个线程调用ConcurrentAlloc，
    for(int i = 0; i < 5; i++){
        void *ptr = ConcurrentAlloc(6);
        cout << "Alloc1: " << ptr << endl;
    }
}

void Alloc2(){
    for(int i = 0; i < 5; i++){
        void *ptr = ConcurrentAlloc(7);
        cout << "Alloc2: " << ptr << endl;
    }
    
}

void AllocTest(){
    cout << "start AllocTest" << endl;
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);
    t1.join();
    t2.join();
    cout << "end AllocTest" << endl;
}


void MultiThreadAlloc1(){
    std::vector<void*> v;
    for(size_t i = 0; i < 7; ++i){
        void* ptr = ConcurrentAlloc(6);
        v.push_back(ptr);
    }
    for(auto ptr : v){
        ConcurrentFree(ptr);
    }
}

void MultiThreadAlloc2(){
    std::vector<void*> v;
    for(size_t i = 0; i < 7; ++i){
        void* ptr = ConcurrentAlloc(16);
        v.push_back(ptr);
    }
    for(auto ptr : v){
        ConcurrentFree(ptr);
    }
}

void TestMultiThreadAlloc(){
    cout << "start TestMultiThreadAlloc" << endl;
    std::thread t1(MultiThreadAlloc1);
    std::thread t2(MultiThreadAlloc2);
    t1.join();
    t2.join();
    cout << "end TestMultiThreadAlloc" << endl;
}

void ConcurrentAllocTest1(){
    cout << "start ConcurrentAllocTest1" << endl;
    void* ptr1 = ConcurrentAlloc(5);
    void* ptr2 = ConcurrentAlloc(16);
    void* ptr3 = ConcurrentAlloc(4);
    void* ptr4 = ConcurrentAlloc(6);
    void* ptr5 = ConcurrentAlloc(3);
    void* ptr6 = ConcurrentAlloc(3);
    void* ptr7 = ConcurrentAlloc(3);

    cout << ptr1 << endl;
    cout << ptr2 << endl;
    cout << ptr3 << endl;
    cout << ptr4 << endl;
    cout << ptr5 << endl;

    ConcurrentFree(ptr1);
    ConcurrentFree(ptr2);
    ConcurrentFree(ptr3);
    ConcurrentFree(ptr4);
    ConcurrentFree(ptr5);
    ConcurrentFree(ptr6);
    ConcurrentFree(ptr7);
    cout << "end ConcurrentAllocTest1" << endl;
}

void ConcurrentAllocTest2(){
    cout << "start ConcurrentAllocTest2" << endl;
    for(int i = 0; i < 1024; ++i){
        void* ptr = ConcurrentAlloc(5);
        cout << ptr << endl;
    }

    void* ptr = ConcurrentAlloc(3);
    cout << "--------" << ptr << endl;
    cout << "end ConcurrentAllocTest2" << endl;
}

int main(int argc, char const *argv[])
{
    
    AllocTest();
    ConcurrentAllocTest1();
    TestMultiThreadAlloc();
    ConcurrentAllocTest2();
    return 0;
}
