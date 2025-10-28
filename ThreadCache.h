#pragma once

#include "Common.h"



class ThreadCache
{
public:
    // 线程分配size大小的空间
    void *Allocate(size_t size);             
    // 线程回收ptr指向的size大小的空间
    void Deallocate(void *ptr, size_t size); 

    // 从中心缓存获取内存
    void* FetchFromCentralCache(size_t index, size_t size); 

    // 向cc归还空间List桶中的空间
    void ListTooLong(FreeList& list, size_t alignSize);
private:
    FreeList _freeLists[FREE_LIST_NUM ]; // 每个桶表示一个自由链表
};


// TLS的全局对象指针，每个线程独立
static __thread ThreadCache *pTLSThreadCache = nullptr; //注意需要是static， 否则会有链接错误

